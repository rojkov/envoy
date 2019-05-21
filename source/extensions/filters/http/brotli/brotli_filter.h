#pragma once

#include "envoy/extensions/filters/http/brotli/v3/brotli.pb.h"

#include "common/compressor/brotli_compressor_impl.h"

#include "extensions/filters/http/common/compressor/compressor.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

/**
 * Configuration for the brotli filter.
 */
class BrotliFilterConfig : public Common::Compressors::CompressorFilterConfig {
public:
  BrotliFilterConfig(const envoy::extensions::filters::http::brotli::v3::Brotli& brotli,
                     const std::string& stats_prefix, Stats::Scope& scope,
                     Runtime::Loader& runtime);

  uint32_t quality() const { return quality_; };
  uint32_t windowBits() const { return window_bits_; }
  uint32_t inputBlockBits() const { return input_block_bits_; }
  bool disableLiteralContextModeling() const { return disable_literal_context_modeling_; }
  Compressor::BrotliCompressorImpl::EncoderMode encoderMode() const { return encoder_mode_; };

  std::unique_ptr<Compressor::Compressor> makeCompressor() override;
  const std::string featureName() const override { return "brotli.filter_enabled"; };

private:
  static Compressor::BrotliCompressorImpl::EncoderMode
  encoderModeEnum(envoy::extensions::filters::http::brotli::v3::Brotli_EncoderMode encoder_mode);

  static uint32_t qualityUint(Protobuf::uint32 quality);
  static uint32_t windowBitsUint(Protobuf::uint32 window_bits);
  static uint32_t inputBlockBitsUint(Protobuf::uint32 input_block_bits);

  // TODO(rojkov): this is going to be deprecated when the old configuration fields are dropped.
  static const envoy::extensions::filters::http::compressor::v3::Compressor
  compressorConfig(const envoy::extensions::filters::http::brotli::v3::Brotli& brotli);

  uint32_t quality_;
  uint32_t window_bits_;
  uint32_t input_block_bits_;
  bool disable_literal_context_modeling_;
  Compressor::BrotliCompressorImpl::EncoderMode encoder_mode_;
};

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
