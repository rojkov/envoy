#pragma once

#include "envoy/config/filter/http/qatzip/v2/qatzip.pb.h"
#include "envoy/config/filter/http/qatzip/v2/qatzip.pb.validate.h"

#include "extensions/filters/http/common/factory_base.h"
#include "extensions/filters/http/well_known_names.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Qatzip {

/**
 * Config registration for the brotli filter. @see NamedHttpFilterConfigFactory.
 */
class QatzipFilterFactory
    : public Common::FactoryBase<envoy::config::filter::http::qatzip::v2::Qatzip> {
public:
  QatzipFilterFactory() : FactoryBase(HttpFilterNames::get().EnvoyQatzip) {}

private:
  Http::FilterFactoryCb
  createFilterFactoryFromProtoTyped(const envoy::config::filter::http::qatzip::v2::Qatzip& config,
                                    const std::string& stats_prefix,
                                    Server::Configuration::FactoryContext& context) override;
};

} // namespace Qatzip
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
