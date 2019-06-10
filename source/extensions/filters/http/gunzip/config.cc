#include "extensions/filters/http/gunzip/config.h"

#include "extensions/filters/http/gunzip/gunzip_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Gunzip {

Http::FilterFactoryCb GunzipFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::gunzip::v2::Gunzip& proto_config,
    const std::string& stats_prefix, Server::Configuration::FactoryContext& context) {
  Common::Decompressors::DecompressorFilterConfigSharedPtr config =
      std::make_shared<GunzipFilterConfig>(proto_config, stats_prefix, context.scope(),
                                           context.runtime());
  return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Common::Decompressors::DecompressorFilter>(config));
  };
}

/**
 * Static registration for the gunzip filter. @see NamedHttpFilterConfigFactory.
 */
REGISTER_FACTORY(GunzipFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Gunzip
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
