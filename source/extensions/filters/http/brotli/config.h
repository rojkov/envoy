#pragma once

#include "envoy/config/filter/http/brotli/v2/brotli.pb.h"
#include "envoy/config/filter/http/brotli/v2/brotli.pb.validate.h"

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

/**
 * Config registration for the brotli filter. @see NamedHttpFilterConfigFactory.
 */
class BrotliFilterFactory : public Common::FactoryBase<envoy::config::filter::http::brotli::v2::Brotli> {
public:
  BrotliFilterFactory() : FactoryBase(HttpFilterNames::get().EnvoyBrotli) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const envoy::config::filter::http::brotli::v2::Brotli& config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
};

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
