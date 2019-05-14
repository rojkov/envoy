#pragma once

#include "envoy/config/filter/http/brotli/v2/brotli.pb.h"
#include "common/common/logger.h"

#include "extensions/filters/http/common/compressor/compressor.h"
#include "common/compressor/brotli_compressor_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

/**
 * Configuration for the brotli filter.
 */
class BrotliFilterConfig : public Common::CompressorFilterConfig {
public:
  BrotliFilterConfig(const envoy::config::filter::http::brotli::v2::Brotli& brotli,
                   const std::string& stats_prefix,
                   Stats::Scope& scope, Runtime::Loader& runtime);

Compressor::BrotliCompressorImpl::EncoderMode encoderMode() const { return encoder_mode_; };

  std::unique_ptr<Compressor::Compressor> getInitializedCompressor() override;
  const std::string featureName() const override { return "brotli.feature_enabled"; };

private:
  static Compressor::BrotliCompressorImpl::EncoderMode encoderModeEnum(
      envoy::config::filter::http::brotli::v2::Brotli_EncoderMode encoder_mode);

  Compressor::BrotliCompressorImpl::EncoderMode encoder_mode_;
};


} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
