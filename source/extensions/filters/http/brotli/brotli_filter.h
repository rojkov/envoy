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

  uint32_t quality() const { return quality_; };
  uint32_t windowBits() const { return window_bits_; }
  uint32_t inputBlockBits() const { return input_block_bits_; }
  Compressor::BrotliCompressorImpl::EncoderMode encoderMode() const { return encoder_mode_; };

  std::unique_ptr<Compressor::Compressor> getInitializedCompressor() override;
  const std::string featureName() const override { return "brotli.feature_enabled"; };

private:
  static Compressor::BrotliCompressorImpl::EncoderMode encoderModeEnum(
      envoy::config::filter::http::brotli::v2::Brotli_EncoderMode encoder_mode);

  static uint32_t qualityUint(Protobuf::uint32 quality);
  static uint32_t windowBitsUint(Protobuf::uint32 window_bits);
  static uint32_t inputBlockBitsUint(Protobuf::uint32 input_block_bits);

  uint32_t quality_;
  uint32_t window_bits_;
  uint32_t input_block_bits_;
  Compressor::BrotliCompressorImpl::EncoderMode encoder_mode_;
};


} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
