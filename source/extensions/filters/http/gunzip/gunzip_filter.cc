#include "extensions/filters/http/gunzip/gunzip_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Gunzip {

namespace {

// When summed to window bits, this makes zlib to decode the gzip format only.
const uint64_t GzipHeaderValue = 16;

} // namespace

GunzipFilterConfig::GunzipFilterConfig(
    const envoy::config::filter::http::gunzip::v2::Gunzip& gunzip, const std::string& stats_prefix,
    Stats::Scope& scope, Runtime::Loader& runtime)
    : DecompressorFilterConfig(gunzip.decompressor(), stats_prefix + "gunzip.", scope, runtime,
                               "gzip"),
      window_bits_(windowBitsUint(gunzip.window_bits().value())) {}

std::unique_ptr<Decompressor::Decompressor> GunzipFilterConfig::makeDecompressor() {
  auto decompressor = std::make_unique<Decompressor::ZlibDecompressorImpl>();
  decompressor->init(windowBits());
  return decompressor;
}

uint64_t GunzipFilterConfig::windowBitsUint(Protobuf::uint32 window_bits) {
  return window_bits | GzipHeaderValue;
}

} // namespace Gunzip
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
