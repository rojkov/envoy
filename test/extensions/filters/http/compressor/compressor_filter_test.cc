#include <memory>

#include "envoy/extensions/filters/http/compressor/v3/compressor.pb.h"

#include "common/protobuf/utility.h"

#include "extensions/filters/http/compressor/compressor_filter.h"

#include "test/mocks/compression/compressor/mocks.h"
#include "test/mocks/http/mocks.h"
#include "test/mocks/protobuf/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Compressor {

using testing::_;
using testing::Return;

class TestCompressorFactory : public Envoy::Compression::Compressor::CompressorFactory {
public:
  TestCompressorFactory(const std::string& content_encoding)
  : content_encoding_(content_encoding) {}

  Envoy::Compression::Compressor::CompressorPtr createCompressor() override {
    auto compressor = std::make_unique<Compression::Compressor::MockCompressor>();
    EXPECT_CALL(*compressor, compress(_, _)).Times(expected_compress_calls_);
    return compressor;
  }

  const std::string& statsPrefix() const override { CONSTRUCT_ON_FIRST_USE(std::string, "test."); }
  const std::string& contentEncoding() const override { return content_encoding_; }

  void setExpectedCompressCalls(uint32_t calls) { expected_compress_calls_ = calls; }

private:
  uint32_t expected_compress_calls_{1};
  const std::string content_encoding_;
};

class CompressorFilterTest : public testing::Test {
public:
  CompressorFilterTest() {
    ON_CALL(runtime_.snapshot_, featureEnabled("test.filter_enabled", 100))
        .WillByDefault(Return(true));
  }

  void SetUp() override {
    setUpFilter(R"EOF(
{
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");
  }

  // CompressorFilter private member functions
  bool hasCacheControlNoTransform(Http::ResponseHeaderMap& headers) {
    return filter_->hasCacheControlNoTransform(headers);
  }

  bool isMinimumContentLength(Http::ResponseHeaderMap& headers) {
    return filter_->isMinimumContentLength(headers);
  }

  bool isTransferEncodingAllowed(Http::ResponseHeaderMap& headers) {
    return filter_->isTransferEncodingAllowed(headers);
  }

  // CompressorFilterTest Helpers
  void setUpFilter(std::string&& json) {
    envoy::extensions::filters::http::compressor::v3::Compressor compressor;
    TestUtility::loadFromJson(json, compressor);
    auto compressor_factory = std::make_unique<TestCompressorFactory>("test");
    compressor_factory_ = compressor_factory.get();
    config_ =
        std::make_shared<CompressorFilterConfig>(compressor, "test.", stats_, runtime_, std::move(compressor_factory));
    filter_ = std::make_unique<CompressorFilter>(config_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
  }

  void verifyCompressedData() {
    EXPECT_EQ(expected_str_.length(), stats_.counter("test.compressor.test.test.total_uncompressed_bytes").value());
    EXPECT_EQ(data_.length(), stats_.counter("test.compressor.test.test.total_compressed_bytes").value());
  }

  void feedBuffer(uint64_t size) {
    TestUtility::feedBufferWithRandomCharacters(data_, size);
    expected_str_ += data_.toString();
  }

  void drainBuffer() {
    const uint64_t data_len = data_.length();
    data_.drain(data_len);
  }

  void doRequest(Http::TestRequestHeaderMapImpl&& headers, bool end_stream) {
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, end_stream));
  }

  void doResponseCompression(Http::TestResponseHeaderMapImpl& headers, bool with_trailers) {
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
    filter_->setDecoderFilterCallbacks(decoder_callbacks);
    uint64_t content_length;
    ASSERT_TRUE(absl::SimpleAtoi(headers.get_("content-length"), &content_length));
    feedBuffer(content_length);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("", headers.get_("content-length"));
    EXPECT_EQ("test", headers.get_("content-encoding"));
    EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, !with_trailers));
    if (with_trailers) {
      Buffer::OwnedImpl trailers_buffer;
      EXPECT_CALL(encoder_callbacks_, addEncodedData(_, true))
          .WillOnce(Invoke([&](Buffer::Instance& data, bool) { data_.move(data); }));
      Http::TestResponseTrailerMapImpl trailers;
      EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(trailers));
    }
    verifyCompressedData();
    drainBuffer();
    EXPECT_EQ(1U, stats_.counter("test.compressor.test.test.compressed").value());
  }

  void doResponseNoCompression(Http::TestResponseHeaderMapImpl& headers) {
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
    filter_->setDecoderFilterCallbacks(decoder_callbacks);
    uint64_t content_length;
    ASSERT_TRUE(absl::SimpleAtoi(headers.get_("content-length"), &content_length));
    feedBuffer(content_length);
    Http::TestResponseHeaderMapImpl continue_headers;
    EXPECT_EQ(Http::FilterHeadersStatus::Continue,
              filter_->encode100ContinueHeaders(continue_headers));
    Http::MetadataMap metadata_map{{"metadata", "metadata"}};
    EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->encodeMetadata(metadata_map));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("", headers.get_("content-encoding"));
    EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, false));
    Http::TestResponseTrailerMapImpl trailers;
    EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(trailers));
    EXPECT_EQ(1, stats_.counter("test.compressor.test.test.not_compressed").value());
  }

  TestCompressorFactory* compressor_factory_;
  std::shared_ptr<CompressorFilterConfig> config_;
  std::unique_ptr<CompressorFilter> filter_;
  Buffer::OwnedImpl data_;
  std::string expected_str_;
  Stats::TestUtil::TestStore stats_;
  NiceMock<Runtime::MockLoader> runtime_;
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
};

// Test if Runtime Feature is Disabled
TEST_F(CompressorFilterTest, DecodeHeadersWithRuntimeDisabled) {
  setUpFilter(R"EOF(
{
  "runtime_enabled": {
    "default_value": true,
    "runtime_key": "foo_key"
  },
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");
  EXPECT_CALL(runtime_.snapshot_, getBoolean("foo_key", true))
      .Times(2)
      .WillRepeatedly(Return(false));
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  doResponseNoCompression(headers);
  EXPECT_FALSE(headers.has("vary"));
}

// Default config values.
TEST_F(CompressorFilterTest, DefaultConfigValues) {
  EXPECT_EQ(30, config_->minimumLength());
  EXPECT_EQ(false, config_->disableOnEtagHeader());
  EXPECT_EQ(false, config_->removeAcceptEncodingHeader());
  EXPECT_EQ(18, config_->contentTypeValues().size());
}

// Acceptance Testing with default configuration.
TEST_F(CompressorFilterTest, AcceptanceTestEncoding) {
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestRequestTrailerMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));

  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  doResponseCompression(headers, false);
}

TEST_F(CompressorFilterTest, AcceptanceTestEncodingWithTrailers) {
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestRequestTrailerMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));
  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  compressor_factory_->setExpectedCompressCalls(2);
  doResponseCompression(headers, true);
}

// Verifies hasCacheControlNoTransform function.
TEST_F(CompressorFilterTest, HasCacheControlNoTransform) {
  {
    Http::TestResponseHeaderMapImpl headers = {{"cache-control", "no-cache"}};
    EXPECT_FALSE(hasCacheControlNoTransform(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"cache-control", "no-transform"}};
    EXPECT_TRUE(hasCacheControlNoTransform(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"cache-control", "No-Transform"}};
    EXPECT_TRUE(hasCacheControlNoTransform(headers));
  }
}

// Verifies that compression is skipped when cache-control header has no-transform value.
TEST_F(CompressorFilterTest, HasCacheControlNoTransformNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test;q=1, deflate"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"cache-control", "no-transform"}};
  doResponseNoCompression(headers);
  EXPECT_FALSE(headers.has("vary"));
}

// Verifies that compression is NOT skipped when cache-control header does NOT have no-transform
// value.
TEST_F(CompressorFilterTest, HasCacheControlNoTransformCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"cache-control", "no-cache"}};
  doResponseCompression(headers, false);
}

TEST_F(CompressorFilterTest, NoAcceptEncodingHeader) {
  doRequest({{":method", "get"}, {}}, true);
  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  doResponseNoCompression(headers);
  EXPECT_EQ(1, stats_.counter("test.compressor.test.test.no_accept_header").value());
  EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
}

TEST_F(CompressorFilterTest, CacheIdentityDecision) {
  // check if identity stat is increased twice (the second time via the cached path).
  compressor_factory_->setExpectedCompressCalls(0);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  doRequest({{":method", "get"}, {"accept-encoding", "identity"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_EQ(1, stats_.counter("test.compressor.test.test.header_identity").value());
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_EQ(2, stats_.counter("test.compressor.test.test.header_identity").value());
}

TEST_F(CompressorFilterTest, CacheHeaderNotValidDecision) {
  // check if not_valid stat is increased twice (the second time via the cached path).
  compressor_factory_->setExpectedCompressCalls(0);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  doRequest({{":method", "get"}, {"accept-encoding", "test;q=invalid"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_EQ(1, stats_.counter("test.compressor.test.test.header_not_valid").value());
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_EQ(2, stats_.counter("test.compressor.test.test.header_not_valid").value());
}

class AcceptEncodingTest : public CompressorFilterTest,
                           public testing::WithParamInterface<std::tuple<std::string, bool, int, int, int, int>> {};

INSTANTIATE_TEST_SUITE_P(AcceptEncodingTestSuite, AcceptEncodingTest,
                         testing::Values(std::make_tuple("deflate, test, br", true, 1, 0, 0, 0),
                                         std::make_tuple("deflate, test;q=1.0, *;q=0.5", true, 1, 0, 0, 0),
                                         std::make_tuple("\tdeflate\t, test\t ; q\t =\t 1.0,\t * ;q=0.5", true, 1, 0, 0, 0),
                                         std::make_tuple("deflate,test;q=1.0,*;q=0", true, 1, 0, 0, 0),
                                         std::make_tuple("deflate, test;q=0.2, br;q=1", true, 1, 0, 0, 0),
                                         std::make_tuple("*", true, 0, 1, 0, 0),
                                         std::make_tuple("*;q=1", true, 0, 1, 0, 0),
                                         std::make_tuple("xyz;q=1, br;q=0.2, *", true, 0, 1, 0, 0),
                                         std::make_tuple("deflate, test;Q=.5, br", true, 1, 0, 0, 0),
                                         std::make_tuple("test;q=0,*;q=1", false, 0, 0, 1, 0),
                                         std::make_tuple("identity, *;q=0", false, 0, 0, 0, 1),
                                         std::make_tuple("identity", false, 0, 0, 0, 1),
                                         std::make_tuple("identity, *;q=0", false, 0, 0, 0, 1),
                                         std::make_tuple("identity;q=1", false, 0, 0, 0, 1),
                                         std::make_tuple("identity;q=0", false, 0, 0, 1, 0),
                                         std::make_tuple("identity;Q=0", false, 0, 0, 1, 0),
                                         std::make_tuple("identity;q=0.5, *;q=0", false, 0, 0, 0, 1),
                                         std::make_tuple("identity;q=0, *;q=0", false, 0, 0, 1, 0),
                                         std::make_tuple("xyz;q=1, br;q=0.2, *;q=0", false, 0, 0, 1, 0),
                                         std::make_tuple("xyz;q=1, br;q=0.2", false, 0, 0, 1, 0),
                                         std::make_tuple("", false, 0, 0, 1, 0),
                                         std::make_tuple("test;q=invalid", false, 0, 0, 1, 0)
                                         ));

TEST_P(AcceptEncodingTest, AcceptEncodingAllowsCompression) {
  std::string accept_encoding = std::get<0>(GetParam());
  bool is_compression_expected = std::get<1>(GetParam());
  int compressor_used = std::get<2>(GetParam());
  int wildcard = std::get<3>(GetParam());
  int not_valid = std::get<4>(GetParam());
  int identity = std::get<5>(GetParam());

  doRequest({{":method", "get"}, {"accept-encoding", accept_encoding}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}};
  if (is_compression_expected) {
    doResponseCompression(headers, false);
  } else {
    doResponseNoCompression(headers);
  }
  EXPECT_EQ(compressor_used, stats_.counter("test.compressor.test.test.header_compressor_used").value());
  EXPECT_EQ(wildcard, stats_.counter("test.compressor.test.test.header_wildcard").value());
  EXPECT_EQ(not_valid, stats_.counter("test.compressor.test.test.header_not_valid").value());
  EXPECT_EQ(identity, stats_.counter("test.compressor.test.test.header_identity").value());
  // Even if compression is disallowed by a client we must let her know the resource is
  // compressible.
  EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
}

TEST(MultipleFiltersTest, IndependentFilters) {
  // The compressor "test1" from an independent filter chain should not overshadow "test2".
  // The independence is simulated with different instances of DecoderFilterCallbacks set for "test1" and "test2".
  NiceMock<Runtime::MockLoader> runtime;
  Stats::TestUtil::TestStore stats1;
  envoy::extensions::filters::http::compressor::v3::Compressor compressor;
  TestUtility::loadFromJson(R"EOF(
{
  "compressor_library": {
     "name": "test1",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF",
                              compressor);
  auto compressor_factory1 = std::make_unique<TestCompressorFactory>("test1");
  compressor_factory1->setExpectedCompressCalls(0);
  auto  config1 =
        std::make_shared<CompressorFilterConfig>(compressor, "test1.", stats1, runtime, std::move(compressor_factory1));
  std::unique_ptr<CompressorFilter> filter1 = std::make_unique<CompressorFilter>(config1);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks1;
  filter1->setDecoderFilterCallbacks(decoder_callbacks1);

  Stats::TestUtil::TestStore stats2;
  TestUtility::loadFromJson(R"EOF(
{
  "compressor_library": {
     "name": "test2",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF",
                              compressor);
  auto compressor_factory2 = std::make_unique<TestCompressorFactory>("test2");
  compressor_factory2->setExpectedCompressCalls(0);
  auto  config2 =
        std::make_shared<CompressorFilterConfig>(compressor, "test2.", stats2, runtime, std::move(compressor_factory2));
  std::unique_ptr<CompressorFilter> filter2 = std::make_unique<CompressorFilter>(config2);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks2;
  filter2->setDecoderFilterCallbacks(decoder_callbacks2);

  Http::TestRequestHeaderMapImpl req_headers{{":method", "get"}, {"accept-encoding", "test1;Q=.5,test2;q=0.75"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->decodeHeaders(req_headers, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->decodeHeaders(req_headers, false));
  Http::TestResponseHeaderMapImpl headers1{
      {":method", "get"}, {"content-length", "256"}};
  Http::TestResponseHeaderMapImpl headers2{
      {":method", "get"}, {"content-length", "256"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->encodeHeaders(headers1, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->encodeHeaders(headers2, false));
  EXPECT_EQ(0, stats1.counter("test1.compressor.test1.test.header_compressor_overshadowed").value());
  EXPECT_EQ(0, stats2.counter("test2.compressor.test2.test.header_compressor_overshadowed").value());
  EXPECT_EQ(1, stats1.counter("test1.compressor.test1.test.compressed").value());
  EXPECT_EQ(1, stats1.counter("test1.compressor.test1.test.header_compressor_used").value());
  EXPECT_EQ(1, stats2.counter("test2.compressor.test2.test.compressed").value());
  EXPECT_EQ(1, stats2.counter("test2.compressor.test2.test.header_compressor_used").value());
}

TEST(MultipleFiltersTest, CacheEncodingDecision) {
  // Test that encoding decision is cached when used by multiple filters.
  Stats::TestUtil::TestStore stats;
  NiceMock<Runtime::MockLoader> runtime;
  envoy::extensions::filters::http::compressor::v3::Compressor compressor;
  TestUtility::loadFromJson(R"EOF(
{
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF",
                            compressor);
  CompressorFilterConfigSharedPtr config1;
  auto compressor_factory1 = std::make_unique<TestCompressorFactory>("test1");
  compressor_factory1->setExpectedCompressCalls(0);
  config1 =
      std::make_shared<CompressorFilterConfig>(compressor, "test1.", stats, runtime, std::move(compressor_factory1));
  std::unique_ptr<CompressorFilter> filter1 = std::make_unique<CompressorFilter>(config1);
  CompressorFilterConfigSharedPtr config2;
  auto compressor_factory2 = std::make_unique<TestCompressorFactory>("test2");
  compressor_factory2->setExpectedCompressCalls(0);
  config2 =
      std::make_shared<CompressorFilterConfig>(compressor, "test2.", stats, runtime, std::move(compressor_factory2));
  std::unique_ptr<CompressorFilter> filter2 = std::make_unique<CompressorFilter>(config2);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter1->setDecoderFilterCallbacks(decoder_callbacks);
  filter2->setDecoderFilterCallbacks(decoder_callbacks);

  Http::TestRequestHeaderMapImpl req_headers{{":method", "get"}, {"accept-encoding", "test1;Q=.5,test2;q=0.75"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->decodeHeaders(req_headers, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->decodeHeaders(req_headers, false));
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->encodeHeaders(headers, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->encodeHeaders(headers, false));
  EXPECT_EQ(1, stats.counter("test1.compressor.test.test.header_compressor_overshadowed").value());
  EXPECT_EQ(1, stats.counter("test2.compressor.test.test.header_compressor_used").value());
  // Reset headers as content-length got removed by filter2.
  headers = {{":method", "get"}, {"content-length", "256"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->encodeHeaders(headers, false));
  EXPECT_EQ(2, stats.counter("test1.compressor.test.test.header_compressor_overshadowed").value());
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->encodeHeaders(headers, false));
  EXPECT_EQ(2, stats.counter("test2.compressor.test.test.header_compressor_used").value());
}

TEST(MultipleFiltersTest, UseFirstRegisteredFilterWhenWildcard) {
  // Test that first registered filter is used when handling wildcard.
  Stats::TestUtil::TestStore stats;
  NiceMock<Runtime::MockLoader> runtime;
  envoy::extensions::filters::http::compressor::v3::Compressor compressor;
  TestUtility::loadFromJson(R"EOF(
{
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF",
                            compressor);
  CompressorFilterConfigSharedPtr config1;
  auto compressor_factory1 = std::make_unique<TestCompressorFactory>("test1");
  compressor_factory1->setExpectedCompressCalls(0);
  config1 =
      std::make_shared<CompressorFilterConfig>(compressor, "test1.", stats, runtime, std::move(compressor_factory1));
  std::unique_ptr<CompressorFilter> filter1 = std::make_unique<CompressorFilter>(config1);
  CompressorFilterConfigSharedPtr config2;
  auto compressor_factory2 = std::make_unique<TestCompressorFactory>("test2");
  compressor_factory2->setExpectedCompressCalls(0);
  config2 =
      std::make_shared<CompressorFilterConfig>(compressor, "test2.", stats, runtime, std::move(compressor_factory2));
  std::unique_ptr<CompressorFilter> filter2 = std::make_unique<CompressorFilter>(config2);
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter1->setDecoderFilterCallbacks(decoder_callbacks);
  filter2->setDecoderFilterCallbacks(decoder_callbacks);

  Http::TestRequestHeaderMapImpl req_headers{{":method", "get"}, {"accept-encoding", "*"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->decodeHeaders(req_headers, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->decodeHeaders(req_headers, false));
  Http::TestResponseHeaderMapImpl headers1{
      {":method", "get"}, {"content-length", "256"}};
  Http::TestResponseHeaderMapImpl headers2{
      {":method", "get"}, {"content-length", "256"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter1->encodeHeaders(headers1, false));
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter2->encodeHeaders(headers2, false));
  EXPECT_EQ(1, stats.counter("test1.compressor.test.test.compressed").value());
  EXPECT_EQ(0, stats.counter("test2.compressor.test.test.compressed").value());
  EXPECT_EQ(1, stats.counter("test1.compressor.test.test.header_wildcard").value());
  EXPECT_EQ(1, stats.counter("test2.compressor.test.test.header_wildcard").value());
}

// Verifies isMinimumContentLength function.
TEST_F(CompressorFilterTest, IsMinimumContentLength) {
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "31"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "29"}};
    EXPECT_FALSE(isMinimumContentLength(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "chunked"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "Chunked"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }

  setUpFilter(R"EOF(
{
  "content_length": 500,
  "compressor_library": {
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "501"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "chunked"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "499"}};
    EXPECT_FALSE(isMinimumContentLength(headers));
  }
}

// Verifies that compression is skipped when content-length header is NOT allowed.
TEST_F(CompressorFilterTest, ContentLengthNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "10"}};
  doResponseNoCompression(headers);
  EXPECT_FALSE(headers.has("vary"));
}

// Verifies that compression is NOT skipped when content-length header is allowed.
TEST_F(CompressorFilterTest, ContentLengthCompression) {
  setUpFilter(R"EOF(
{
  "content_length": 500,
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "1000"}};
  doResponseCompression(headers, false);
}

// Verifies that compression is skipped when content-type header is NOT allowed.
TEST_F(CompressorFilterTest, ContentTypeNoCompression) {
  setUpFilter(R"EOF(
    {
      "content_type": [
        "text/html",
        "text/css",
        "text/plain",
        "application/javascript",
        "application/json",
        "font/eot",
        "image/svg+xml"
      ],
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
    }
  )EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"content-type", "image/jpeg"}};
  doResponseNoCompression(headers);
  EXPECT_EQ(1, stats_.counter("test.compressor.test.test.header_not_valid").value());
  // Assert the resource is not compressible.
  EXPECT_FALSE(headers.has("vary"));
}

// Verifies that compression is NOT skipped when content-encoding header is allowed.
TEST_F(CompressorFilterTest, ContentTypeCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{{":method", "get"},
                                          {"content-length", "256"},
                                          {"content-type", "application/json;charset=utf-8"}};
  doResponseCompression(headers, false);
}

// Verifies isTransferEncodingAllowed function.
TEST_F(CompressorFilterTest, IsTransferEncodingAllowed) {
  {
    Http::TestResponseHeaderMapImpl headers = {};
    EXPECT_TRUE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "chunked"}};
    EXPECT_TRUE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "Chunked"}};
    EXPECT_TRUE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "deflate"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "Deflate"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "test"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", "test, chunked"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"transfer-encoding", " test\t,  chunked\t"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
}

// Tests compression when Transfer-Encoding header exists.
TEST_F(CompressorFilterTest, TransferEncodingChunked) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"transfer-encoding", "chunked"}};
  doResponseCompression(headers, false);
}

// Tests compression when Transfer-Encoding header exists.
TEST_F(CompressorFilterTest, AcceptanceTransferEncoding) {

  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"transfer-encoding", "chunked, deflate"}};
  doResponseNoCompression(headers);
  EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
}

// Content-Encoding: upstream response is already encoded.
TEST_F(CompressorFilterTest, ContentEncodingAlreadyEncoded) {
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl response_headers{
      {":method", "get"}, {"content-length", "256"}, {"content-encoding", "deflate, gzip"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
  EXPECT_TRUE(response_headers.has("content-length"));
  EXPECT_FALSE(response_headers.has("transfer-encoding"));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, false));
}

// No compression when upstream response is empty.
TEST_F(CompressorFilterTest, EmptyResponse) {

  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {":status", "204"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, true));
  EXPECT_EQ("", headers.get_("content-length"));
  EXPECT_EQ("", headers.get_("content-encoding"));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, true));
}

// Verifies insertVaryHeader function.
TEST_F(CompressorFilterTest, InsertVaryHeader) {
  {
    Http::TestResponseHeaderMapImpl headers{
      {"content-length", "256"}};
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "256"}, {"vary", "Cookie"}};
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("Cookie, Accept-Encoding", headers.get_("vary"));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "256"}, {"vary", "accept-encoding"}};
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("accept-encoding, Accept-Encoding", headers.get_("vary"));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "256"}, {"vary", "Accept-Encoding, Cookie"}};
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("Accept-Encoding, Cookie", headers.get_("vary"));
  }
  {
    Http::TestResponseHeaderMapImpl headers = {{"content-length", "256"}, {"vary", "Accept-Encoding"}};
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
  }
}

// Filter should set Vary header value with `accept-encoding`.
TEST_F(CompressorFilterTest, NoVaryHeader) {
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  doResponseCompression(headers, false);
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
}

// Filter should set Vary header value with `accept-encoding` and preserve other values.
TEST_F(CompressorFilterTest, VaryOtherValues) {
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"vary", "User-Agent, Cookie"}};
  doResponseCompression(headers, false);
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("User-Agent, Cookie, Accept-Encoding", headers.get_("vary"));
}

// Vary header should have only one `accept-encoding`value.
TEST_F(CompressorFilterTest, VaryAlreadyHasAcceptEncoding) {
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"vary", "accept-encoding"}};
  doResponseCompression(headers, false);
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("accept-encoding, Accept-Encoding", headers.get_("vary"));
}

// Verify removeAcceptEncoding header.
TEST_F(CompressorFilterTest, RemoveAcceptEncodingHeader) {
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  filter_->setDecoderFilterCallbacks(decoder_callbacks);
  {
    Http::TestRequestHeaderMapImpl headers = {{"accept-encoding", "deflate, test, gzip, br"}};
    setUpFilter(R"EOF(
{
  "remove_accept_encoding_header": true,
  "compressor_library": {
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_FALSE(headers.has("accept-encoding"));
  }
  {
    Http::TestRequestHeaderMapImpl headers = {{"accept-encoding", "deflate, test, gzip, br"}};
    setUpFilter(R"EOF(
{
  "compressor_library": {
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_TRUE(headers.has("accept-encoding"));
    EXPECT_EQ("deflate, test, gzip, br", headers.get_("accept-encoding"));
  }
}

class IsContentTypeAllowedTest : public CompressorFilterTest,
                                 public testing::WithParamInterface<std::tuple<std::string, bool, bool>> {
};

INSTANTIATE_TEST_SUITE_P(IsContentTypeAllowedDefault,
                         IsContentTypeAllowedTest,
                         testing::Values(std::make_tuple("text/html", true, false),
                                         std::make_tuple("text/xml", true, false),
                                         std::make_tuple("text/plain", true, false),
                                         std::make_tuple("text/css", true, false),
                                         std::make_tuple("application/javascript", true, false),
                                         std::make_tuple("application/x-javascript", true, false),
                                         std::make_tuple("text/javascript", true, false),
                                         std::make_tuple("text/x-javascript", true, false),
                                         std::make_tuple("text/ecmascript", true, false),
                                         std::make_tuple("text/js", true, false),
                                         std::make_tuple("text/jscript", true, false),
                                         std::make_tuple("text/x-js", true, false),
                                         std::make_tuple("application/ecmascript", true, false),
                                         std::make_tuple("application/x-json", true, false),
                                         std::make_tuple("application/xml", true, false),
                                         std::make_tuple("application/json", true, false),
                                         std::make_tuple("image/svg+xml", true, false),
                                         std::make_tuple("application/xhtml+xml", true, false),
                                         std::make_tuple("application/json;charset=utf-8", true, false),
                                         std::make_tuple("Application/XHTML+XML", true, false),
                                         std::make_tuple("\ttext/html\t", true, false),
                                         std::make_tuple("image/jpeg", false, false),
                                         std::make_tuple("xyz/svg+xml", true, true),
                                         std::make_tuple("xyz/false", false, true),
                                         std::make_tuple("image/jpeg", false, true),
                                         std::make_tuple("test/insensitive", true, true)));

// Verifies isContentTypeAllowed function.
TEST_P(IsContentTypeAllowedTest, IsContentTypeAllowed) {
  std::string content_type = std::get<0>(GetParam());
  bool should_compress = std::get<1>(GetParam());
  bool is_custom_config = std::get<2>(GetParam());

  if (is_custom_config) {
    setUpFilter(R"EOF(
      {
        "content_type": [
          "text/html",
          "xyz/svg+xml",
          "Test/INSENSITIVE"
        ],
    "compressor_library": {
       "name": "test",
       "typed_config": {
         "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
       }
    }
      }
    )EOF");
  }

  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"content-type", content_type}};
  if (should_compress) {
    doResponseCompression(headers, false);
  } else {
    doResponseNoCompression(headers);
  }
}

class CompressWithEtagTest : public CompressorFilterTest,
                             public testing::WithParamInterface<std::tuple<std::string, std::string, bool>> {
};

INSTANTIATE_TEST_SUITE_P(CompressWithEtagSuite, CompressWithEtagTest,
                         testing::Values(std::make_tuple("etag", R"EOF(W/"686897696a7c876b7e")EOF", true),
                                         std::make_tuple("etag", R"EOF(w/"686897696a7c876b7e")EOF", true),
                                         std::make_tuple("etag", "686897696a7c876b7e", false),
                                         std::make_tuple("x-garbage", "garbagevalue", false)));

TEST_P(CompressWithEtagTest, CompressionIsEnabledOnEtag) {
  std::string header_name = std::get<0>(GetParam());
  std::string header_value = std::get<1>(GetParam());
  bool is_weak_etag = std::get<2>(GetParam());

  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {header_name, header_value}};
  doResponseCompression(headers, false);
  EXPECT_EQ(0, stats_.counter("test.compressor.test.test.not_compressed_etag").value());
  EXPECT_EQ("test", headers.get_("content-encoding"));
  if (is_weak_etag) {
    EXPECT_EQ(header_value, headers.get_("etag"));
  } else {
    EXPECT_FALSE(headers.has("etag"));
  }
}

TEST_P(CompressWithEtagTest, CompressionIsDisabledOnEtag) {
  std::string header_name = std::get<0>(GetParam());
  std::string header_value = std::get<1>(GetParam());

  setUpFilter(R"EOF(
{
  "disable_on_etag_header": true,
  "compressor_library": {
     "name": "test",
     "typed_config": {
       "@type": "type.googleapis.com/envoy.extensions.compression.gzip.compressor.v3.Gzip"
     }
  }
}
)EOF");

  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  Http::TestResponseHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {header_name, header_value}};
  if (StringUtil::CaseInsensitiveCompare()("etag", header_name)) {
    doResponseNoCompression(headers);
    EXPECT_EQ(1, stats_.counter("test.compressor.test.test.not_compressed_etag").value());
    EXPECT_FALSE(headers.has("vary"));
    EXPECT_TRUE(headers.has("etag"));
  } else {
    doResponseCompression(headers, false);
    EXPECT_EQ(0, stats_.counter("test.compressor.test.test.not_compressed_etag").value());
    EXPECT_EQ("test", headers.get_("content-encoding"));
    EXPECT_TRUE(headers.has("vary"));
    EXPECT_FALSE(headers.has("etag"));
  }
}

} // namespace Compressor
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
