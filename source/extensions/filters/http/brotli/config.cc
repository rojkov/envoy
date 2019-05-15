#include "extensions/filters/http/brotli/config.h"


#include "extensions/filters/http/brotli/brotli_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

Http::FilterFactoryCb BrotliFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::config::filter::http::brotli::v2::Brotli& proto_config,
    const std::string& stats_prefix, Server::Configuration::FactoryContext& context) {
  Common::Compressors::CompressorFilterConfigSharedPtr config = std::make_shared<BrotliFilterConfig>(
      proto_config, stats_prefix, context.scope(), context.runtime());
  return [config](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<Common::Compressors::CompressorFilter>(config));
  };
}

/**
 * Static registration for the brotli filter. @see NamedHttpFilterConfigFactory.
 */
REGISTER_FACTORY(BrotliFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
