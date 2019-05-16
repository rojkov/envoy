#include "common/protobuf/utility.h"

#include "extensions/filters/http/common/compressor/compressor.h"
#include "test/extensions/filters/http/common/compressor/compressor.pb.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Common {
namespace Compressors {

class MockCompressorFilterConfig : public CompressorFilterConfig {
public:
  MockCompressorFilterConfig(const test::extensions::filters::http::common::compressor::Mock& mock,
                             const std::string& stats_prefix, Stats::Scope& scope,
                             Runtime::Loader& runtime)
    : CompressorFilterConfig(mock.content_length().value(),
                             mock.content_type(),
                             mock.disable_on_etag_header(),
                             mock.remove_accept_encoding_header(),
                             stats_prefix + "test.", scope, runtime, "test") {}

  MOCK_METHOD0(makeCompressor, std::unique_ptr<Compressor::Compressor>());
  //MOCK_CONST_METHOD0(featureName, const std::string());
  const std::string featureName() const { return "test.filter_enabled"; }
};

class CompressorFilterTestFixture : public testing::Test {
protected:
  void SetUpFilter(std::string&& json) {
    Json::ObjectSharedPtr config = Json::Factory::loadFromString(json);
    test::extensions::filters::http::common::compressor::Mock mock;
    MessageUtil::loadFromJson(json, mock);
    config_.reset(new MockCompressorFilterConfig(mock, "test.", stats_, runtime_));
    filter_ = std::make_unique<CompressorFilter>(config_);
  }

  CompressorFilterConfigSharedPtr config_;
  std::unique_ptr<CompressorFilter> filter_;
  Stats::IsolatedStoreImpl stats_;
  NiceMock<Runtime::MockLoader> runtime_;
};

// Test if Runtime Feature is Disabled
TEST_F(CompressorFilterTestFixture, RuntimeDisabled) {
  SetUpFilter("{}");
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("test.filter_enabled", 100))
      .WillOnce(Return(false));

  Http::TestHeaderMapImpl headers{{":method", "get"}, {"accept-encoding", "deflate, gzip"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, false));
  EXPECT_EQ(1, stats_.counter("test.test.not_compressed").value());
}

} // namespace Compressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
