#include "extensions/filters/http/common/decompressor/decompressor.h"

#include "common/buffer/buffer_impl.h"
#include "common/http/header_map_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {
namespace Decompressors {

DecompressorFilterConfig::DecompressorFilterConfig(
    const envoy::config::filter::http::decompressor::v2::Decompressor& decompressor,
    const std::string& stats_prefix, Stats::Scope& scope, Runtime::Loader& runtime,
    const std::string& content_encoding)
    : decompression_direction_(decompressionDirectionEnum(decompressor.decompression_direction())),
      stats_(generateStats(stats_prefix, scope)), runtime_(runtime),
      content_encoding_(content_encoding) {}

DecompressionDirection DecompressorFilterConfig::decompressionDirectionEnum(
    envoy::config::filter::http::decompressor::v2::Decompressor_DecompressionDirection
        decompression_direction) {
  switch (decompression_direction) {
  case envoy::config::filter::http::decompressor::v2::Decompressor_DecompressionDirection_RESPONSE:
    return DecompressionDirection::Response;
  case envoy::config::filter::http::decompressor::v2::Decompressor_DecompressionDirection_REQUEST:
    return DecompressionDirection::Request;
  case envoy::config::filter::http::decompressor::v2::
      Decompressor_DecompressionDirection_RESPONSE_AND_REQUEST:
    return DecompressionDirection::ResponseAndRequest;
  default:
    return DecompressionDirection::Request;
  }
}

bool DecompressorFilterConfig::isRequestDecompressionEnabled() const {
  return decompression_direction_ != DecompressionDirection::Response;
}

bool DecompressorFilterConfig::isResponseDecompressionEnabled() const {
  return decompression_direction_ != DecompressionDirection::Request;
}

DecompressorFilter::DecompressorFilter(DecompressorFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

Http::FilterHeadersStatus DecompressorFilter::decodeHeaders(Http::HeaderMap& headers,
                                                            bool /* end_stream */) {
  if (config_->isResponseDecompressionEnabled()) {
    injectAcceptEncoding(headers);
    printf(__FILE__ ":%d * decodeHeaders() added Accept-Encoding: %s\n", __LINE__,
           config_->contentEncoding().c_str());
  }

  if (config_->isRequestDecompressionEnabled() &&
      config_->runtime().snapshot().featureEnabled(config_->featureName(), 100) &&
      !hasCacheControlNoTransform(headers) && isContentEncodingAllowed(headers)) {
    printf(__FILE__ ":%d * decodeHeaders() do decompress request\n", __LINE__);
    request_decompressor_ = config_->makeDecompressor();
    removeContentEncoding(headers);
  }

  return Http::FilterHeadersStatus::Continue;
};
Http::FilterDataStatus DecompressorFilter::decodeData(Buffer::Instance& buffer,
                                                      bool /* end_stream */) {
  printf(__FILE__ ":%d * decodeData() request_decompressor_:%d\n", __LINE__,
         !!request_decompressor_);
  if (request_decompressor_) {
    Buffer::OwnedImpl output_buffer;
    request_decompressor_->decompress(buffer, output_buffer);
    buffer.drain(buffer.length());
    buffer.add(output_buffer);
  }

  return Http::FilterDataStatus::Continue;
}

Http::FilterHeadersStatus DecompressorFilter::encodeHeaders(Http::HeaderMap& headers,
                                                            bool end_stream) {
  printf(__FILE__ ":%d * encodeHeaders() end_stream: %d\n", __LINE__, end_stream);
  if (end_stream) {
    return Http::FilterHeadersStatus::Continue;
  }

  if (config_->isResponseDecompressionEnabled() &&
      config_->runtime().snapshot().featureEnabled(config_->featureName(), 100) &&
      !hasCacheControlNoTransform(headers) && isContentEncodingAllowed(headers)) {
    printf(__FILE__ ":%d * encodeHeaders() do decompress response\n", __LINE__);
    decompressor_ = config_->makeDecompressor();
    removeContentEncoding(headers);
    // TODO: sanitize Transfer-Encoding
  } else {
    config_->stats().not_decompressed_.inc();
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus DecompressorFilter::encodeData(Buffer::Instance& data, bool end_stream) {
  printf(__FILE__ ":%d * encodeData() decompressor_:%d length:%lu end_stream:%d\n", __LINE__,
         !!decompressor_, data.length(), end_stream);
  if (decompressor_) {
    Buffer::OwnedImpl output_buffer;
    decompressor_->decompress(data, output_buffer);
    data.drain(data.length());
    data.add(output_buffer);
  }

  return Http::FilterDataStatus::Continue;
}

bool DecompressorFilter::hasCacheControlNoTransform(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* cache_control = headers.CacheControl();
  if (!cache_control) {
    return false;
  }

  return StringUtil::caseFindToken(cache_control->value().getStringView(), ",",
                                   Http::Headers::get().CacheControlValues.NoTransform);
}

bool DecompressorFilter::isContentEncodingAllowed(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* content_encoding = headers.ContentEncoding();
  if (!content_encoding) {
    printf(__FILE__ ":%d * isContentEncodingAllowed() false\n", __LINE__);
    return false;
  }

  absl::string_view coding =
      StringUtil::trim(StringUtil::cropRight(content_encoding->value().getStringView(), ","));
  if (StringUtil::caseCompare(config_->contentEncoding(), coding)) {
    printf(__FILE__ ":%d * isContentEncodingAllowed() true\n", __LINE__);
    return true;
  }

  printf(__FILE__ ":%d * isContentEncodingAllowed() false\n", __LINE__);
  return false;
}

void DecompressorFilter::removeContentEncoding(Http::HeaderMap& headers) const {
  const auto all_codings = headers.ContentEncoding()->value().getStringView();
  const auto codings = StringUtil::cropLeft(all_codings, ",");

  if (codings != all_codings) {
    const auto remaining_encodings = std::string(StringUtil::trim(codings));
    headers.removeContentEncoding();
    headers.insertContentEncoding().value(remaining_encodings);
  } else {
    headers.removeContentEncoding();
  }
}

// TODO(rojkov): inject encoding with configurable qvalue with q=1 by default.
void DecompressorFilter::injectAcceptEncoding(Http::HeaderMap& headers) const {
  const Http::HeaderEntry* accept_encoding = headers.AcceptEncoding();

  if (!accept_encoding) {
    headers.insertAcceptEncoding().value(config_->contentEncoding());
    return;
  }

  std::vector<std::string> encodings{config_->contentEncoding()};
  for (const auto token : StringUtil::splitToken(accept_encoding->value().getStringView(), ",",
                                                 false /* keep_empty */)) {
    const auto encoding = StringUtil::trim(StringUtil::cropRight(token, ";"));
    if (!StringUtil::caseCompare(config_->contentEncoding(), encoding)) {
      encodings.push_back(std::string(token));
    }
  }

  headers.removeAcceptEncoding();
  headers.insertAcceptEncoding().value(StringUtil::join(encodings, ","));
}

} // namespace Decompressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy