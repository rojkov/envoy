#include "extensions/filters/http/common/compressor/compressor.h"

#include "common/common/lock_guard.h"
#include "common/http/header_map_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {
namespace Compressors {

namespace {

// Default minimum length of an upstream response that allows compression.
const uint64_t DefaultMinimumContentLength = 30;

// Default content types will be used if any is provided by the user.
const std::vector<std::string>& defaultContentEncoding() {
  CONSTRUCT_ON_FIRST_USE(
      std::vector<std::string>,
      {"text/html", "text/plain", "text/css", "application/javascript", "application/x-javascript",
       "text/javascript", "text/x-javascript", "text/ecmascript", "text/js", "text/jscript",
       "text/x-js", "application/ecmascript", "application/x-json", "application/xml",
       "application/json", "image/svg+xml", "text/xml", "application/xhtml+xml"});
}

// List of CompressorFilterConfig objects registered for a stream.
struct CompressorRegistry : public StreamInfo::FilterState::Object {
  std::list<CompressorFilterConfigSharedPtr> filter_configs_;
};

// Key to per stream CompressorRegistry objects.
const std::string& compressorRegistryKey() { CONSTRUCT_ON_FIRST_USE(std::string, "compressors"); }

} // namespace

CompressorFilterConfig::CompressorFilterConfig(
    const envoy::extensions::filters::http::compressor::v3::Compressor& compressor,
    const std::string& stats_prefix, Stats::Scope& scope, Runtime::Loader& runtime,
    const std::string& content_encoding)
    : content_length_(contentLengthUint(compressor.content_length().value())),
      content_type_values_(contentTypeSet(compressor.content_type())),
      disable_on_etag_header_(compressor.disable_on_etag_header()),
      remove_accept_encoding_header_(compressor.remove_accept_encoding_header()),
      stats_(generateStats(stats_prefix, scope)), runtime_(runtime),
      content_encoding_(content_encoding) {}

StringUtil::CaseUnorderedSet
CompressorFilterConfig::contentTypeSet(const Protobuf::RepeatedPtrField<std::string>& types) {
  const auto& default_content_encodings = defaultContentEncoding();
  return types.empty() ? StringUtil::CaseUnorderedSet(default_content_encodings.begin(),
                                                      default_content_encodings.end())
                       : StringUtil::CaseUnorderedSet(types.cbegin(), types.cend());
}

uint32_t CompressorFilterConfig::contentLengthUint(Protobuf::uint32 length) {
  return length > 0 ? length : DefaultMinimumContentLength;
}

CompressorFilter::CompressorFilter(CompressorFilterConfigSharedPtr config)
    : skip_compression_{true}, compressor_(), config_(std::move(config)) {}

Http::FilterHeadersStatus CompressorFilter::decodeHeaders(Http::HeaderMap& headers, bool) {
  const Http::HeaderEntry* accept_encoding = headers.AcceptEncoding();
  if (accept_encoding != nullptr) {
    accept_encoding_ = std::make_unique<std::string>(accept_encoding->value().getStringView());
  }

  if (config_->runtime().snapshot().featureEnabled(config_->featureName(), 100)) {
    skip_compression_ = false;
    if (config_->removeAcceptEncodingHeader()) {
      headers.removeAcceptEncoding();
    }
  } else {
    config_->stats().not_compressed_.inc();
  }

  return Http::FilterHeadersStatus::Continue;
}

void CompressorFilter::setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) {
  decoder_callbacks_ = &callbacks;

  absl::string_view key = compressorRegistryKey();
  const StreamInfo::FilterStateSharedPtr& filter_state = callbacks.streamInfo().filterState();
  if (filter_state->hasData<CompressorRegistry>(key)) {
    CompressorRegistry& registry = filter_state->getDataMutable<CompressorRegistry>(key);
    registry.filter_configs_.push_back(config_);
  } else {
    auto registry_ptr = std::make_unique<CompressorRegistry>();
    registry_ptr->filter_configs_.push_back(config_);
    filter_state->setData(key, std::move(registry_ptr),
                          StreamInfo::FilterState::StateType::Mutable);
  }
}

Http::FilterHeadersStatus CompressorFilter::encodeHeaders(Http::HeaderMap& headers,
                                                          bool end_stream) {
  const Http::HeaderEntry* accept_encoding = headers.AcceptEncoding();
  if (accept_encoding_ != nullptr) {
    std::cout << config_->contentEncoding() << " REQUEST: " << *accept_encoding_ << std::endl;
  } else {
    std::cout << config_->contentEncoding() << " REQUEST: n/a" << std::endl;
  }
  if (accept_encoding) {
    std::cout << config_->contentEncoding()
              << " RESPONSE: " << accept_encoding->value().getStringView() << std::endl;
  } else {
    std::cout << config_->contentEncoding() << " RESPONSE: n/a" << std::endl;
  }

  if (!end_stream && !skip_compression_ && isMinimumContentLength(headers) &&
      isAcceptEncodingAllowed(headers) && isContentTypeAllowed(headers) &&
      !hasCacheControlNoTransform(headers) && isEtagAllowed(headers) &&
      isTransferEncodingAllowed(headers) && !headers.ContentEncoding()) {
    sanitizeEtagHeader(headers);
    insertVaryHeader(headers);
    headers.removeContentLength();
    headers.setContentEncoding(config_->contentEncoding());
    config_->stats().compressed_.inc();
    compressor_ = config_->makeCompressor();
  } else if (!skip_compression_) {
    skip_compression_ = true;
    config_->stats().not_compressed_.inc();
  }
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus CompressorFilter::encodeData(Buffer::Instance& data, bool end_stream) {
  if (!skip_compression_) {
    config_->stats().total_uncompressed_bytes_.add(data.length());
    compressor_->compress(data, end_stream ? Compressor::State::Finish : Compressor::State::Flush);
    config_->stats().total_compressed_bytes_.add(data.length());
  }
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus CompressorFilter::encodeTrailers(Http::HeaderMap&) {
  if (!skip_compression_) {
    Buffer::OwnedImpl empty_buffer;
    compressor_->compress(empty_buffer, Compressor::State::Finish);
    config_->stats().total_compressed_bytes_.add(empty_buffer.length());
    encoder_callbacks_->addEncodedData(empty_buffer, true);
  }
  return Http::FilterTrailersStatus::Continue;
}

bool CompressorFilter::hasCacheControlNoTransform(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* cache_control = headers.CacheControl();
  if (cache_control) {
    return StringUtil::caseFindToken(cache_control->value().getStringView(), ",",
                                     Http::Headers::get().CacheControlValues.NoTransform);
  }

  return false;
}

std::unique_ptr<CompressorFilter::EncodingDecision>
CompressorFilter::chooseEncoding(const Http::HeaderMap& headers) const {
  using EncPair = std::pair<absl::string_view, float>; // pair of {encoding, q_value}
  std::vector<EncPair> pairs;
  std::string content_type_value{};

  const Http::HeaderEntry* content_type = headers.ContentType();
  if (content_type != nullptr) {
    content_type_value = std::string(
        StringUtil::trim(StringUtil::cropRight(content_type->value().getStringView(), ";")));
  }

  // There could be many compressors registered for the same content encoding, e.g. consider a case
  // when there are two gzip filters using different compression levels for different content sizes.
  // In such case we ignore duplicates (or different filters for the same encoding) registered last.
  std::map<std::string, uint32_t> allowed_compressors;
  uint32_t registration_count{0};
  for (const auto& filter_config :
       decoder_callbacks_->streamInfo()
           .filterState()
           ->getDataReadOnly<CompressorRegistry>(compressorRegistryKey())
           .filter_configs_) {
    if (!content_type_value.empty() && !filter_config->contentTypeValues().empty()) {
      auto type = filter_config->contentTypeValues().find(content_type_value);
      if (type == filter_config->contentTypeValues().end()) {
        std::cout << config_->contentEncoding() << " chooseEncoding(): skipped "
                  << content_type_value << " for " << filter_config->contentEncoding() << std::endl;
        continue;
      }
    }
    auto enc = allowed_compressors.find(filter_config->contentEncoding());
    if (enc == allowed_compressors.end()) {
      std::cout << config_->contentEncoding() << " chooseEncoding(): added to allowed_compressors "
                << filter_config->contentEncoding() << std::endl;
      allowed_compressors.insert({filter_config->contentEncoding(), registration_count});
      ++registration_count;
    }
  }

  for (const auto token : StringUtil::splitToken(*accept_encoding_, ",", false /* keep_empty */)) {
    EncPair pair = std::make_pair(StringUtil::trim(StringUtil::cropRight(token, ";")), 1);
    const auto params = StringUtil::cropLeft(token, ";");
    if (params != token) {
      const auto q_value = StringUtil::cropLeft(params, "=");
      if (q_value != params &&
          StringUtil::caseCompare("q", StringUtil::trim(StringUtil::cropRight(params, "=")))) {
        auto result = absl::SimpleAtof(StringUtil::trim(q_value), &pair.second);
        if (!result) {
          continue;
        }
      }
    }

    pairs.push_back(pair);

    // Disallow compressors with "q=0".
    // The reason why we add encodings to "pairs" even with "q=0" is that "pairs" contains
    // client's expectations and "allowed_compressors" is what the server can handle. Consider
    // the cases of "Accept-Encoding: gzip;q=0, deflate, *" and "Accept-Encoding: deflate, *"
    // whereas the server has only "gzip" configured. If we just exclude the encodings with "q=0"
    // from "pairs" then upon noticing "*" we don't know if "gzip" is acceptable by the client.
    if (!pair.second) {
      allowed_compressors.erase(std::string(pair.first));
    }
  }

  if (pairs.empty()) {
    // If the Accept-Encoding field-value is empty, then only the "identity" encoding is acceptable.
    config_->stats().header_not_valid_.inc();
    std::cout << config_->contentEncoding() << " chooseEncoding(): 220" << std::endl;
    return std::make_unique<CompressorFilter::EncodingDecision>(
        Http::Headers::get().AcceptEncodingValues.Identity,
        CompressorFilter::EncodingDecision::HeaderStat::NotValid);
  }

  EncPair choice{Http::Headers::get().AcceptEncodingValues.Identity, 0};
  for (const auto pair : pairs) {
    std::cout << config_->contentEncoding() << " pair: " << pair.first << " " << pair.second
              << " choice: " << choice.first << " " << choice.second;
    std::cout << " allowed_compressors size: " << allowed_compressors.size()
              << " allowed_compressors.count(std::string(pair.first)):"
              << allowed_compressors.count(std::string(pair.first)) << std::endl;
    for (auto t : allowed_compressors) {
      std::cout << "allowed compressor: " << t.first << std::endl;
    }
    if ((pair.second > choice.second) &&
        (allowed_compressors.count(std::string(pair.first)) ||
         pair.first == Http::Headers::get().AcceptEncodingValues.Identity ||
         pair.first == Http::Headers::get().AcceptEncodingValues.Wildcard)) {
      choice = pair;
    }
  }

  if (!choice.second) {
    config_->stats().header_not_valid_.inc();
    std::cout << config_->contentEncoding() << " chooseEncoding(): 238 header not valid"
              << std::endl;
    return std::make_unique<CompressorFilter::EncodingDecision>(
        Http::Headers::get().AcceptEncodingValues.Identity,
        CompressorFilter::EncodingDecision::HeaderStat::NotValid);
  }

  // The "identity" encoding (no compression) is always available.
  if (choice.first == Http::Headers::get().AcceptEncodingValues.Identity) {
    config_->stats().header_identity_.inc();
    std::cout << config_->contentEncoding() << " chooseEncoding(): 247" << std::endl;
    return std::make_unique<CompressorFilter::EncodingDecision>(
        Http::Headers::get().AcceptEncodingValues.Identity,
        CompressorFilter::EncodingDecision::HeaderStat::Identity);
  }

  // If wildcard is given then use which ever compressor is registered first.
  if (choice.first == Http::Headers::get().AcceptEncodingValues.Wildcard) {
    if (!allowed_compressors.empty()) {
      config_->stats().header_wildcard_.inc();
      auto first_registered = std::min_element(
          allowed_compressors.begin(), allowed_compressors.end(),
          [](const std::pair<std::string, uint32_t>& a,
             const std::pair<std::string, uint32_t>& b) -> bool { return a.second < b.second; });
      return std::make_unique<CompressorFilter::EncodingDecision>(
          first_registered->first, CompressorFilter::EncodingDecision::HeaderStat::Wildcard);
    }
  }

  if (StringUtil::caseCompare(config_->contentEncoding(), choice.first)) {
    config_->stats().header_compressor_used_.inc();
    // TODO(rojkov): Remove this increment when the gzip-specific stat is gone.
    if (StringUtil::caseCompare("gzip", choice.first)) {
      config_->stats().header_gzip_.inc();
    }
    return std::make_unique<CompressorFilter::EncodingDecision>(
        std::string(choice.first), CompressorFilter::EncodingDecision::HeaderStat::Used);
  } else if (!allowed_compressors.empty()) {
    config_->stats().header_compressor_overshadowed_.inc();
    return std::make_unique<CompressorFilter::EncodingDecision>(
        std::string(choice.first), CompressorFilter::EncodingDecision::HeaderStat::Overshadowed);
  }

  config_->stats().header_not_valid_.inc();
  std::cout << config_->contentEncoding() << " chooseEncoding(): 281" << std::endl;
  return std::make_unique<CompressorFilter::EncodingDecision>(
      Http::Headers::get().AcceptEncodingValues.Identity,
      CompressorFilter::EncodingDecision::HeaderStat::NotValid);
}

bool CompressorFilter::isAcceptEncodingAllowed(const Http::HeaderMap& headers) const {
  if (accept_encoding_ == nullptr) {
    config_->stats().no_accept_header_.inc();
    return false;
  }

  const absl::string_view encoding_decision_key{"encoding_decision"};

  // Check if we have already cached our decision on encoding.
  const StreamInfo::FilterStateSharedPtr& filter_state =
      decoder_callbacks_->streamInfo().filterState();
  if (filter_state->hasData<CompressorFilter::EncodingDecision>(encoding_decision_key)) {
    const CompressorFilter::EncodingDecision& decision =
        filter_state->getDataReadOnly<CompressorFilter::EncodingDecision>(encoding_decision_key);
    if (StringUtil::caseCompare(config_->contentEncoding(), decision.encoding())) {
      config_->stats().header_compressor_used_.inc();
      // TODO(rojkov): Remove this increment when the gzip-specific stat is gone.
      if (StringUtil::caseCompare("gzip", config_->contentEncoding())) {
        config_->stats().header_gzip_.inc();
      }
      return true;
    }

    switch (decision.stat()) {
    case CompressorFilter::EncodingDecision::HeaderStat::Identity:
      config_->stats().header_identity_.inc();
      break;
    case CompressorFilter::EncodingDecision::HeaderStat::Wildcard:
      config_->stats().header_wildcard_.inc();
      break;
    case CompressorFilter::EncodingDecision::HeaderStat::NotValid:
      config_->stats().header_not_valid_.inc();
      break;
    default:
      config_->stats().header_compressor_overshadowed_.inc();
    }

    return false;
  }

  std::unique_ptr<CompressorFilter::EncodingDecision> decision = chooseEncoding(headers);
  std::cout << config_->contentEncoding() << " chosen encoding:" << decision->encoding()
            << std::endl;
  bool result = StringUtil::caseCompare(config_->contentEncoding(), decision->encoding());
  filter_state->setData(encoding_decision_key, std::move(decision),
                        StreamInfo::FilterState::StateType::ReadOnly);
  return result;
}

bool CompressorFilter::isContentTypeAllowed(Http::HeaderMap& headers) const {
  std::cout << config_->contentEncoding()
            << " config size of content_types: " << config_->contentTypeValues().size()
            << std::endl;
  const Http::HeaderEntry* content_type = headers.ContentType();
  if (content_type) {
    std::cout << config_->contentEncoding()
              << " received Content-Type: " << content_type->value().getStringView() << std::endl;
  }
  if (content_type != nullptr && !config_->contentTypeValues().empty()) {
    const absl::string_view value =
        StringUtil::trim(StringUtil::cropRight(content_type->value().getStringView(), ";"));
    std::cout << config_->contentEncoding() << " isContentTypeAllowed():"
              << (config_->contentTypeValues().find(value) != config_->contentTypeValues().end())
              << std::endl;
    return config_->contentTypeValues().find(value) != config_->contentTypeValues().end();
  }

  return true;
}

bool CompressorFilter::isEtagAllowed(Http::HeaderMap& headers) const {
  const bool is_etag_allowed = !(config_->disableOnEtagHeader() && headers.Etag());
  if (!is_etag_allowed) {
    config_->stats().not_compressed_etag_.inc();
  }
  return is_etag_allowed;
}

bool CompressorFilter::isMinimumContentLength(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* content_length = headers.ContentLength();
  if (content_length != nullptr) {
    uint64_t length;
    const bool is_minimum_content_length =
        absl::SimpleAtoi(content_length->value().getStringView(), &length) &&
        length >= config_->minimumLength();
    if (!is_minimum_content_length) {
      config_->stats().content_length_too_small_.inc();
    }
    return is_minimum_content_length;
  }

  const Http::HeaderEntry* transfer_encoding = headers.TransferEncoding();
  return (transfer_encoding &&
          StringUtil::caseFindToken(transfer_encoding->value().getStringView(), ",",
                                    Http::Headers::get().TransferEncodingValues.Chunked));
}

bool CompressorFilter::isTransferEncodingAllowed(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* transfer_encoding = headers.TransferEncoding();
  if (transfer_encoding != nullptr) {
    for (absl::string_view header_value :
         StringUtil::splitToken(transfer_encoding->value().getStringView(), ",", true)) {
      const auto trimmed_value = StringUtil::trim(header_value);
      if (StringUtil::caseCompare(trimmed_value, config_->contentEncoding()) ||
          // or any other compression type known to Envoy
          StringUtil::caseCompare(trimmed_value,
                                  Http::Headers::get().TransferEncodingValues.Gzip) ||
          StringUtil::caseCompare(trimmed_value,
                                  Http::Headers::get().TransferEncodingValues.Brotli) ||
          StringUtil::caseCompare(trimmed_value,
                                  Http::Headers::get().TransferEncodingValues.Deflate)) {
        return false;
      }
    }
  }

  return true;
}

void CompressorFilter::insertVaryHeader(Http::HeaderMap& headers) {
  const Http::HeaderEntry* vary = headers.Vary();
  if (vary != nullptr) {
    if (!StringUtil::findToken(vary->value().getStringView(), ",",
                               Http::Headers::get().VaryValues.AcceptEncoding, true)) {
      std::string new_header;
      absl::StrAppend(&new_header, vary->value().getStringView(), ", ",
                      Http::Headers::get().VaryValues.AcceptEncoding);
      headers.setVary(new_header);
    }
  } else {
    headers.setReferenceVary(Http::Headers::get().VaryValues.AcceptEncoding);
  }
}

// TODO(gsagula): It seems that every proxy has a different opinion how to handle Etag. Some
// discussions around this topic have been going on for over a decade, e.g.,
// https://bz.apache.org/bugzilla/show_bug.cgi?id=45023
// This design attempts to stay more on the safe side by preserving weak etags and removing
// the strong ones when disable_on_etag_header is false. Envoy does NOT re-write entity tags.
void CompressorFilter::sanitizeEtagHeader(Http::HeaderMap& headers) {
  const Http::HeaderEntry* etag = headers.Etag();
  if (etag != nullptr) {
    absl::string_view value(etag->value().getStringView());
    if (value.length() > 2 && !((value[0] == 'w' || value[0] == 'W') && value[1] == '/')) {
      headers.removeEtag();
    }
  }
}

} // namespace Compressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
