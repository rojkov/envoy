#pragma once

#include "envoy/config/filter/http/decompressor/v2/decompressor.pb.h"
#include "envoy/decompressor/decompressor.h"
#include "envoy/http/filter.h"
#include "envoy/runtime/runtime.h"

#include "common/common/macros.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {
namespace Decompressors {

enum class DecompressionDirection {
  Response,
  Request,
  ResponseAndRequest,
};

// TODO: add counters and actually use them
// clang-format off
#define ALL_DECOMPRESSOR_STATS(COUNTER)    \
  COUNTER(decompressed)                    \
  COUNTER(not_decompressed)                \
// clang-format on

/**
 * Struct definition for compressor stats. @see stats_macros.h
 */
struct DecompressorStats {
  ALL_DECOMPRESSOR_STATS(GENERATE_COUNTER_STRUCT)
};

class DecompressorFilterConfig {
public:
  DecompressorFilterConfig() = delete;
  virtual ~DecompressorFilterConfig() {};

  virtual const std::string featureName() const PURE;
  virtual std::unique_ptr<Decompressor::Decompressor> makeDecompressor() PURE;

  Runtime::Loader& runtime() { return runtime_; }
  DecompressorStats& stats() { return stats_; }
  const std::string contentEncoding() const { return content_encoding_; };
  bool isRequestDecompressionEnabled() const;
  bool isResponseDecompressionEnabled() const;

protected:
  DecompressorFilterConfig(const envoy::config::filter::http::decompressor::v2::Decompressor& decompressor,
                           const std::string& stats_prefix,
                           Stats::Scope& scope,
                           Runtime::Loader& runtime,
                           const std::string& content_encoding);

private:
  static DecompressionDirection decompressionDirectionEnum(
    envoy::config::filter::http::decompressor::v2::Decompressor_DecompressionDirection decompression_direction);

  static DecompressorStats generateStats(const std::string& prefix, Stats::Scope& scope) {
    return DecompressorStats{ALL_DECOMPRESSOR_STATS(POOL_COUNTER_PREFIX(scope, prefix))};
  }

  DecompressionDirection decompression_direction_;
  DecompressorStats stats_;
  Runtime::Loader& runtime_;
  std::string content_encoding_;
};
using DecompressorFilterConfigSharedPtr = std::shared_ptr<DecompressorFilterConfig>;

/**
 * A filter that decompresses data dispatched from the upstream upon client request.
 */
class DecompressorFilter : public Http::StreamFilter {
public:
  DecompressorFilter(DecompressorFilterConfigSharedPtr config);

  // Http::StreamFilterBase
  void onDestroy() override{};

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::HeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance& buffer, bool end_stream) override;
  Http::FilterTrailersStatus decodeTrailers(Http::HeaderMap&) override {
    return Http::FilterTrailersStatus::Continue;
  }
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
    decoder_callbacks_ = &callbacks;
  };

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encode100ContinueHeaders(Http::HeaderMap&) override {
    return Http::FilterHeadersStatus::Continue;
  }
  Http::FilterHeadersStatus encodeHeaders(Http::HeaderMap& /* headers */, bool /* end_stream */) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& buffer, bool end_stream) override;
  Http::FilterTrailersStatus encodeTrailers(Http::HeaderMap&) override {
    return Http::FilterTrailersStatus::Continue;
  }
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap&) override {
    return Http::FilterMetadataStatus::Continue;
  }
  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks& callbacks) override {
    encoder_callbacks_ = &callbacks;
  }

private:
  bool hasCacheControlNoTransform(Http::HeaderMap& headers) const;
  bool isContentEncodingAllowed(Http::HeaderMap& headers) const;
  void removeContentEncoding(Http::HeaderMap& headers) const;

  DecompressorFilterConfigSharedPtr config_;
  std::unique_ptr<Decompressor::Decompressor> decompressor_;
  std::unique_ptr<Decompressor::Decompressor> request_decompressor_;

  Http::StreamDecoderFilterCallbacks* decoder_callbacks_{nullptr};
  Http::StreamEncoderFilterCallbacks* encoder_callbacks_{nullptr};
};

  // Http::StreamDecoderFilter
} // namespace Decompressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy