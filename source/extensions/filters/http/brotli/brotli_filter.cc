#include "extensions/filters/http/brotli/brotli_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

namespace {

// Default brotli quality.
const uint32_t DefaultQuality = 11;

} // namespace

BrotliFilterConfig::BrotliFilterConfig(const envoy::config::filter::http::brotli::v2::Brotli& brotli,
                                   const std::string& stats_prefix, Stats::Scope& scope,
                                   Runtime::Loader& runtime)
    : CompressorFilterConfig(brotli.content_length().value(),
                             brotli.content_type(),
                             brotli.disable_on_etag_header(),
                             brotli.remove_accept_encoding_header(),
                             stats_prefix + "brotli.", scope, runtime, "br"),
      quality_(qualityUint(brotli.quality().value())),
      encoder_mode_(encoderModeEnum(brotli.encoder_mode())) {
  ENVOY_LOG(warn, "** COMPRESSOR NAME: brotli. mode: {}", static_cast<uint32_t>(encoder_mode_));
}

Compressor::BrotliCompressorImpl::EncoderMode BrotliFilterConfig::encoderModeEnum(
    envoy::config::filter::http::brotli::v2::Brotli_EncoderMode encoder_mode) {
  switch (encoder_mode) {
  case envoy::config::filter::http::brotli::v2::Brotli_EncoderMode::Brotli_EncoderMode_GENERIC:
    return Compressor::BrotliCompressorImpl::EncoderMode::Generic;
  case envoy::config::filter::http::brotli::v2::Brotli_EncoderMode::Brotli_EncoderMode_TEXT:
    return Compressor::BrotliCompressorImpl::EncoderMode::Text;
  case envoy::config::filter::http::brotli::v2::Brotli_EncoderMode::Brotli_EncoderMode_FONT:
    return Compressor::BrotliCompressorImpl::EncoderMode::Font;
  default:
    return Compressor::BrotliCompressorImpl::EncoderMode::Default;
  }
}

uint32_t BrotliFilterConfig::qualityUint(Protobuf::uint32 quality) {
  return quality > 0 ? quality - 1 : DefaultQuality;
}

std::unique_ptr<Compressor::Compressor> BrotliFilterConfig::getInitializedCompressor() {
  auto compressor = std::make_unique<Compressor::BrotliCompressorImpl>();
  compressor->init(quality(), encoderMode());
  return compressor;
}

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
