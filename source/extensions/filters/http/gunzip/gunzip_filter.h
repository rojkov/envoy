#pragma once

#include "envoy/config/filter/http/gunzip/v2/gunzip.pb.h"

#include "common/decompressor/zlib_decompressor_impl.h"

#include "extensions/filters/http/common/decompressor/decompressor.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Gunzip {

/**
 * Configuration for the gunzip filter.
 */
class GunzipFilterConfig : public Common::Decompressors::DecompressorFilterConfig {

public:
  GunzipFilterConfig(const envoy::config::filter::http::gunzip::v2::Gunzip& gunzip,
                     const std::string& stats_prefix, Stats::Scope& scope,
                     Runtime::Loader& runtime);

  const std::string featureName() const override { return "gunzip.filter_enabled"; };
  std::unique_ptr<Decompressor::Decompressor> makeDecompressor() override;

  uint64_t windowBits() const { return window_bits_; }

private:
  static uint64_t windowBitsUint(Protobuf::uint32 window_bits);

  int32_t window_bits_;
};

} // namespace Gunzip
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
