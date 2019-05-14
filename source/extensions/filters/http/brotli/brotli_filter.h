#pragma once

#include "envoy/config/filter/http/brotli/v2/brotli.pb.h"
#include "common/common/logger.h"

#include "extensions/filters/http/common/compressor/compressor.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

/**
 * Configuration for the brotli filter.
 */
class BrotliFilterConfig : public Common::CompressorFilterConfig, protected Logger::Loggable<Logger::Id::connection> {
public:
  BrotliFilterConfig(const envoy::config::filter::http::brotli::v2::Brotli& brotli,
                   const std::string& stats_prefix,
                   Stats::Scope& scope, Runtime::Loader& runtime);
  std::unique_ptr<Compressor::Compressor> getInitializedCompressor() override;
  const std::string featureName() const override { return "brotli.feature_enabled"; };
};


} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
