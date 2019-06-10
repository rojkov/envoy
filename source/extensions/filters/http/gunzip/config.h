#pragma once

#include "envoy/config/filter/http/gunzip/v2/gunzip.pb.h"
#include "envoy/config/filter/http/gunzip/v2/gunzip.pb.validate.h"

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Gunzip {

/**
 * Config registration for the gzip filter. @see NamedHttpFilterConfigFactory.
 */
class GunzipFilterFactory
    : public Common::FactoryBase<envoy::config::filter::http::gunzip::v2::Gunzip> {
public:
  GunzipFilterFactory() : FactoryBase(HttpFilterNames::get().EnvoyGunzip) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const envoy::config::filter::http::gunzip::v2::Gunzip& config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
};

} // namespace Gunzip
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
