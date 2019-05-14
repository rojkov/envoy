#include "extensions/filters/http/brotli/brotli_filter.h"

#include "common/compressor/brotli_compressor_impl.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

BrotliFilterConfig::BrotliFilterConfig(const envoy::config::filter::http::brotli::v2::Brotli& brotli,
                                   const std::string& stats_prefix, Stats::Scope& scope,
                                   Runtime::Loader& runtime)
    : CompressorFilterConfig(brotli.content_length().value(),
                             brotli.content_type(),
                             brotli.disable_on_etag_header(),
                             brotli.remove_accept_encoding_header(),
                             stats_prefix + "brotli.", scope, runtime, "br"){
        ENVOY_LOG(warn, "** COMPRESSOR NAME: brotli");
      }

std::unique_ptr<Compressor::Compressor> BrotliFilterConfig::getInitializedCompressor() {
  auto compressor = std::make_unique<Compressor::BrotliCompressorImpl>();
  compressor->init();
  return compressor;
}

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
