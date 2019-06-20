#include "extensions/filters/http/common/decompressor/decompressor.h"

#include "common/buffer/buffer_impl.h"
#include "common/http/header_map_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {
namespace Decompressors {

DecompressorFilterConfig::DecompressorFilterConfig(const std::string& stats_prefix,
                                                   Stats::Scope& scope, Runtime::Loader& runtime,
                                                   const std::string& content_encoding)
    : stats_(generateStats(stats_prefix, scope)), runtime_(runtime),
      content_encoding_(content_encoding) {}

DecompressorFilter::DecompressorFilter(DecompressorFilterConfigSharedPtr config)
    : config_(std::move(config)) {}

Http::FilterHeadersStatus DecompressorFilter::decodeHeaders(Http::HeaderMap& headers,
                                                            bool /* end_stream */) {
  // TODO: inject proper Accept-Encoding header
  headers.removeAcceptEncoding();
  headers.insertAcceptEncoding().value(config_->contentEncoding());
  return Http::FilterHeadersStatus::Continue;
};

Http::FilterHeadersStatus DecompressorFilter::encodeHeaders(Http::HeaderMap& headers,
                                                            bool end_stream) {
  printf(__FILE__ ":%d * encodeHeaders() end_stream: %d\n", __LINE__, end_stream);
  if (end_stream) {
    return Http::FilterHeadersStatus::Continue;
  }

  if (config_->runtime().snapshot().featureEnabled(config_->featureName(), 100) &&
      !hasCacheControlNoTransform(headers) && isContentEncodingAllowed(headers)) {
    printf(__FILE__ ":%d * encodeHeaders()\n", __LINE__);
    decompressor_ = config_->makeDecompressor();
    const auto all_codings = headers.ContentEncoding()->value().getStringView();
    const auto codings = StringUtil::cropLeft(all_codings, ",");
    if (codings != all_codings) {
      const auto remaining_encodings = std::string(StringUtil::trim(codings));
      headers.removeContentEncoding();
      headers.insertContentEncoding().value(remaining_encodings);
    } else {
      headers.removeContentEncoding();
    }
    // TODO: sanitize Transfer-Encoding
  } else {
    config_->stats().not_decompressed_.inc();
  }

  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus DecompressorFilter::encodeData(Buffer::Instance& data,
                                                      bool /* end_stream */) {
  printf(__FILE__ ":%d * encodeData() decompressor_:%d\n", __LINE__, !!decompressor_);
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
  if (StringUtil::caseCompare("gzip", coding)) {
  printf(__FILE__ ":%d * isContentEncodingAllowed() true\n", __LINE__);
    return true;
  }

  printf(__FILE__ ":%d * isContentEncodingAllowed() false\n", __LINE__);
  return false;
}

} // namespace Decompressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy