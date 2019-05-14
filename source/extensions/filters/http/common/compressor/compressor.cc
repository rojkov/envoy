#include "extensions/filters/http/common/compressor/compressor.h"

#include "common/http/header_map_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {

namespace {

// Minimum length of an upstream response that allows compression.
const uint64_t MinimumContentLength = 30;

// Default content types will be used if any is provided by the user.
const std::vector<std::string>& defaultContentEncoding() {
  CONSTRUCT_ON_FIRST_USE(std::vector<std::string>,
                         {"text/html", "text/plain", "text/css", "application/javascript",
                          "application/json", "image/svg+xml", "text/xml",
                          "application/xhtml+xml"});
}

} // namespace

std::vector<std::string> CompressorFilterConfig::registered_compressors_;

StringUtil::CaseUnorderedSet CompressorFilterConfig::contentTypeSet(
    const Protobuf::RepeatedPtrField<Envoy::ProtobufTypes::String>& types) {
  return types.empty() ? StringUtil::CaseUnorderedSet(defaultContentEncoding().begin(),
                                                      defaultContentEncoding().end())
                       : StringUtil::CaseUnorderedSet(types.cbegin(), types.cend());
}

uint64_t CompressorFilterConfig::contentLengthUint(Protobuf::uint32 length) {
  return length >= MinimumContentLength ? length : MinimumContentLength;
}

CompressorFilter::CompressorFilter(const CompressorFilterConfigSharedPtr& config)
    : skip_compression_{true}, compressed_data_(), compressor_(nullptr), config_(config) {}

Http::FilterHeadersStatus CompressorFilter::decodeHeaders(Http::HeaderMap& headers, bool) {
  if (config_->runtime().snapshot().featureEnabled(config_->featureName(), 100) && isAcceptEncodingAllowed(headers)) {
    compressor_ = config_->getInitializedCompressor();
    skip_compression_ = false;
    if (config_->removeAcceptEncodingHeader()) {
      headers.removeAcceptEncoding();
    }
  } else {
    config_->stats().not_compressed_.inc();
  }

  return Http::FilterHeadersStatus::Continue;
}

bool CompressorFilter::isAcceptEncodingAllowed(const Http::HeaderMap& headers) const {
  const Http::HeaderEntry* accept_encoding = headers.AcceptEncoding();
  if (!accept_encoding) {
    config_->stats().no_accept_header_.inc();
    return false;
  }

  typedef std::pair<absl::string_view, float> encPair; // pair of {encoding, q_value}
  std::vector<encPair> pairs;

  for (const auto token : StringUtil::splitToken(accept_encoding->value().getStringView(), ",", false /* keep_empty */)) {
      encPair pair = std::make_pair(StringUtil::trim(StringUtil::cropRight(token, ";")), 1);
      const auto params = StringUtil::cropLeft(token, ";");
      if (params != token) {
        const auto q_value = StringUtil::cropLeft(params, "=");
        if (q_value != params && StringUtil::caseCompare("q", StringUtil::trim(StringUtil::cropRight(params, "=")))) {
          // TODO: drop ASSERT from production code
          ASSERT(absl::SimpleAtof(StringUtil::trim(q_value), &pair.second));
        }
      }
      pairs.push_back(pair);
  }

  std::sort(pairs.begin(), pairs.end(), [](const encPair &a, const encPair &b) -> bool {
    return a.second > b.second;
  });

  if (pairs.size() == 0) {
      // If the Accept-Encoding field-value is empty, then only the "identity" encoding is acceptable.
      config_->stats().header_identity_.inc();
      return false;
  }

  for (const auto pair : pairs) {
    for (const auto compr : config_->registeredCompressors()) {
      if (StringUtil::caseCompare(pair.first, compr) && pair.second > 0) {
        // In case a user specified more than one encodings with the same quality value
        // select the one which is registered first in Envoy's config.
        if (StringUtil::caseCompare(config_->contentEncoding(), compr)) {
          config_->stats().header_compressor_used_.inc();
          return true;
        } else {
          config_->stats().header_compressor_overshadowed_.inc();
          return false;
        }
      }
    }

    // The "identity" encoding (no compression) is always avalable.
    if (pair.first == Http::Headers::get().AcceptEncodingValues.Identity) {
      if (pair.second > 0) {
        config_->stats().header_identity_.inc();
      } else {
        config_->stats().header_not_valid_.inc();
      }
      return false;
    }

    // If wildcard is given then use which ever compressor is registered first.
    if (pair.first == Http::Headers::get().AcceptEncodingValues.Wildcard) {
      if (pair.second > 0) {
        config_->stats().header_wildcard_.inc();
        return StringUtil::caseCompare(config_->contentEncoding(), config_->registeredCompressors()[0]);
      } else {
        config_->stats().header_not_valid_.inc();
        return false;
      }
    }
  }

  // As per https://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.3
  // "The 'identity' content-coding is always acceptable, unless specifically refused".
  config_->stats().header_identity_.inc();
  return false;
}

Http::FilterHeadersStatus CompressorFilter::encodeHeaders(Http::HeaderMap& headers, bool end_stream) {
  if (!end_stream && !skip_compression_ && isMinimumContentLength(headers) &&
      isContentTypeAllowed(headers) && !hasCacheControlNoTransform(headers) &&
      isEtagAllowed(headers) && isTransferEncodingAllowed(headers) && !headers.ContentEncoding()) {
    sanitizeEtagHeader(headers);
    insertVaryHeader(headers);
    headers.removeContentLength();
    headers.insertContentEncoding().value(config_->contentEncoding());
    config_->stats().compressed_.inc();
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

bool CompressorFilter::hasCacheControlNoTransform(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* cache_control = headers.CacheControl();
  if (cache_control) {
    return StringUtil::caseFindToken(cache_control->value().getStringView(), ",",
                                     Http::Headers::get().CacheControlValues.NoTransform);
  }

  return false;
}

bool CompressorFilter::isContentTypeAllowed(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* content_type = headers.ContentType();
  if (content_type && !config_->contentTypeValues().empty()) {
    const absl::string_view value =
        StringUtil::trim(StringUtil::cropRight(content_type->value().getStringView(), ";"));
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
  if (content_length) {
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
  if (transfer_encoding) {
    for (auto header_value :
         // TODO(gsagula): add Http::HeaderMap::string_view() so string length doesn't need to be
         // computed twice. Find all other sites where this can be improved.
         StringUtil::splitToken(transfer_encoding->value().getStringView(), ",", true)) {
      const auto trimmed_value = StringUtil::trim(header_value);
      if (StringUtil::caseCompare(trimmed_value,
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
  if (vary) {
    if (!StringUtil::findToken(vary->value().getStringView(), ",",
                               Http::Headers::get().VaryValues.AcceptEncoding, true)) {
      std::string new_header;
      absl::StrAppend(&new_header, vary->value().getStringView(), ", ",
                      Http::Headers::get().VaryValues.AcceptEncoding);
      headers.insertVary().value(new_header);
    }
  } else {
    headers.insertVary().value(Http::Headers::get().VaryValues.AcceptEncoding);
  }
}

// TODO(gsagula): It seems that every proxy has a different opinion how to handle Etag. Some
// discussions around this topic have been going on for over a decade, e.g.,
// https://bz.apache.org/bugzilla/show_bug.cgi?id=45023
// This design attempts to stay more on the safe side by preserving weak etags and removing
// the strong ones when disable_on_etag_header is false. Envoy does NOT re-write entity tags.
void CompressorFilter::sanitizeEtagHeader(Http::HeaderMap& headers) {
  const Http::HeaderEntry* etag = headers.Etag();
  if (etag) {
    absl::string_view value(etag->value().getStringView());
    if (value.length() > 2 && !((value[0] == 'w' || value[0] == 'W') && value[1] == '/')) {
      headers.removeEtag();
    }
  }
}

} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy