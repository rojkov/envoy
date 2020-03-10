#include "extensions/filters/http/brotli/brotli_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

namespace {

// Default brotli quality.
const uint32_t DefaultQuality = 11;

// Default and maximum compression window size.
const uint64_t DefaultWindowBits = 24;

// Default and maximum compression input block size.
const uint64_t DefaultInputBlockBits = 24;

} // namespace

BrotliFilterConfig::BrotliFilterConfig(
    const envoy::extensions::filters::http::brotli::v3::Brotli& brotli,
    const std::string& stats_prefix, Stats::Scope& scope, Runtime::Loader& runtime)
	: CompressorFilterConfig(brotli.compressor(), stats_prefix + "brotli.", scope, runtime,
                             "br"),
      quality_(qualityUint(brotli.quality().value())),
      window_bits_(windowBitsUint(brotli.window_bits().value())),
      input_block_bits_(inputBlockBitsUint(brotli.input_block_bits().value())),
      disable_literal_context_modeling_(brotli.disable_literal_context_modeling()),
      encoder_mode_(encoderModeEnum(brotli.encoder_mode())) {}

Compressor::BrotliCompressorImpl::EncoderMode BrotliFilterConfig::encoderModeEnum(
    envoy::extensions::filters::http::brotli::v3::Brotli_EncoderMode encoder_mode) {
  switch (encoder_mode) {
  case envoy::extensions::filters::http::brotli::v3::Brotli_EncoderMode::Brotli_EncoderMode_GENERIC:
    return Compressor::BrotliCompressorImpl::EncoderMode::Generic;
  case envoy::extensions::filters::http::brotli::v3::Brotli_EncoderMode::Brotli_EncoderMode_TEXT:
    return Compressor::BrotliCompressorImpl::EncoderMode::Text;
  case envoy::extensions::filters::http::brotli::v3::Brotli_EncoderMode::Brotli_EncoderMode_FONT:
    return Compressor::BrotliCompressorImpl::EncoderMode::Font;
  default:
    return Compressor::BrotliCompressorImpl::EncoderMode::Default;
  }
}

uint32_t BrotliFilterConfig::qualityUint(Protobuf::uint32 quality) {
  return quality > 0 ? quality - 1 : DefaultQuality;
}

uint32_t BrotliFilterConfig::windowBitsUint(Protobuf::uint32 window_bits) {
  return window_bits > 0 ? window_bits : DefaultWindowBits;
}

uint32_t BrotliFilterConfig::inputBlockBitsUint(Protobuf::uint32 input_block_bits) {
  return input_block_bits > 0 ? input_block_bits : DefaultInputBlockBits;
}

std::unique_ptr<Compressor::Compressor> BrotliFilterConfig::makeCompressor() {
  auto compressor = std::make_unique<Compressor::BrotliCompressorImpl>();
  compressor->init(quality(), windowBits(), inputBlockBits(), disableLiteralContextModeling(),
                   encoderMode());
  return compressor;
}

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
