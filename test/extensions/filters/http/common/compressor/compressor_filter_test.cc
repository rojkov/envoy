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

class MockCompressor : public Compressor::Compressor {
  void compress(Buffer::Instance&, ::Envoy::Compressor::State) {}
};

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

  // MOCK_METHOD0(makeCompressor, std::unique_ptr<Compressor::Compressor>());
  //MOCK_CONST_METHOD0(featureName, const std::string());
  std::unique_ptr<Compressor::Compressor> makeCompressor() { return std::make_unique<MockCompressor>(); }
  const std::string featureName() const { return "test.filter_enabled"; }
};

class CompressorFilterTest : public testing::Test {
protected:
  CompressorFilterTest() {
    ON_CALL(runtime_.snapshot_, featureEnabled("test.filter_enabled", 100))
        .WillByDefault(Return(true));
  }

  // CompressorFilter private member functions
  void sanitizeEtagHeader(Http::HeaderMap& headers) { filter_->sanitizeEtagHeader(headers); }

  void insertVaryHeader(Http::HeaderMap& headers) { filter_->insertVaryHeader(headers); }

  bool isContentTypeAllowed(Http::HeaderMap& headers) {
    return filter_->isContentTypeAllowed(headers);
  }

  bool isEtagAllowed(Http::HeaderMap& headers) { return filter_->isEtagAllowed(headers); }

  bool hasCacheControlNoTransform(Http::HeaderMap& headers) {
    return filter_->hasCacheControlNoTransform(headers);
  }

  bool isAcceptEncodingAllowed(Http::HeaderMap& headers) {
    return filter_->isAcceptEncodingAllowed(headers);
  }

  bool isMinimumContentLength(Http::HeaderMap& headers) {
    return filter_->isMinimumContentLength(headers);
  }

  bool isTransferEncodingAllowed(Http::HeaderMap& headers) {
    return filter_->isTransferEncodingAllowed(headers);
  }

  // CompressorFilterTest Helpers
  void SetUpFilter(std::string&& json) {
    Json::ObjectSharedPtr config = Json::Factory::loadFromString(json);
    test::extensions::filters::http::common::compressor::Mock mock;
    MessageUtil::loadFromJson(json, mock);
    config_.reset(new MockCompressorFilterConfig(mock, "test.", stats_, runtime_));
    filter_ = std::make_unique<CompressorFilter>(config_);
  }

  void verifyCompressedData() {
    //std::cout << "IIII: " << expected_str_.length() << " data:" << data_.length() << std::endl;
    EXPECT_EQ(expected_str_.length(), stats_.counter("test.test.total_uncompressed_bytes").value());
    EXPECT_EQ(data_.length(), stats_.counter("test.test.total_compressed_bytes").value());
  }

  void feedBuffer(uint64_t size) {
    TestUtility::feedBufferWithRandomCharacters(data_, size);
    expected_str_ += data_.toString();
  }

  void drainBuffer() {
    const uint64_t data_len = data_.length();
    data_.drain(data_len);
  }

  void doRequest(Http::TestHeaderMapImpl&& headers, bool end_stream) {
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, end_stream));
  }

  void doResponseCompression(Http::TestHeaderMapImpl&& headers) {
    uint64_t content_length;
    ASSERT_TRUE(absl::SimpleAtoi(headers.get_("content-length"), &content_length));
    feedBuffer(content_length);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("", headers.get_("content-length"));
    EXPECT_EQ("test", headers.get_("content-encoding"));
    EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, true));
    verifyCompressedData();
    drainBuffer();
    EXPECT_EQ(1U, stats_.counter("test.test.compressed").value());
  }

  void doResponseNoCompression(Http::TestHeaderMapImpl&& headers) {
    uint64_t content_length;
    ASSERT_TRUE(absl::SimpleAtoi(headers.get_("content-length"), &content_length));
    feedBuffer(content_length);
    Http::TestHeaderMapImpl continue_headers;
    EXPECT_EQ(Http::FilterHeadersStatus::Continue,
              filter_->encode100ContinueHeaders(continue_headers));
    Http::MetadataMap metadata_map{{"metadata", "metadata"}};
    EXPECT_EQ(Http::FilterMetadataStatus::Continue, filter_->encodeMetadata(metadata_map));
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
    EXPECT_EQ("", headers.get_("content-encoding"));
    EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, false));
    Http::TestHeaderMapImpl trailers;
    EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(trailers));
    EXPECT_EQ(1, stats_.counter("test.test.not_compressed").value());
  }

  CompressorFilterConfigSharedPtr config_;
  std::unique_ptr<CompressorFilter> filter_;
  Buffer::OwnedImpl data_;
  std::string expected_str_;
  Stats::IsolatedStoreImpl stats_;
  NiceMock<Runtime::MockLoader> runtime_;
};

// Test if Runtime Feature is Disabled
TEST_F(CompressorFilterTest, DecodeHeadersWithRuntimeDisabled) {
  SetUpFilter("{}");
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("test.filter_enabled", 100))
      .WillOnce(Return(false));

  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  doResponseNoCompression({{":method", "get"}, {"content-length", "256"}});
}

// Default config values.
TEST_F(CompressorFilterTest, DefaultConfigValues) {
  SetUpFilter("{}");
  EXPECT_EQ(30, config_->minimumLength());
  EXPECT_EQ(false, config_->disableOnEtagHeader());
  EXPECT_EQ(false, config_->removeAcceptEncodingHeader());
  EXPECT_EQ(8, config_->contentTypeValues().size());
}

// Acceptance Testing with default configuration.
TEST_F(CompressorFilterTest, AcceptanceTestEncoding) {
  SetUpFilter("{}");
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestHeaderMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));
  doResponseCompression({{":method", "get"}, {"content-length", "256"}});
}

// Verifies hasCacheControlNoTransform function.
TEST_F(CompressorFilterTest, hasCacheControlNoTransform) {
  {
    Http::TestHeaderMapImpl headers = {{"cache-control", "no-cache"}};
    EXPECT_FALSE(hasCacheControlNoTransform(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"cache-control", "no-transform"}};
    EXPECT_TRUE(hasCacheControlNoTransform(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"cache-control", "No-Transform"}};
    EXPECT_TRUE(hasCacheControlNoTransform(headers));
  }
}

// Verifies that compression is skipped when cache-control header has no-transform value.
TEST_F(CompressorFilterTest, hasCacheControlNoTransformNoCompression) {
  SetUpFilter("{}");
  doRequest({{":method", "get"}, {"accept-encoding", "test;q=0, deflate"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"cache-control", "no-transform"}});
}

// Verifies that compression is NOT skipped when cache-control header does NOT have no-transform
// value.
TEST_F(CompressorFilterTest, hasCacheControlNoTransformCompression) {
  SetUpFilter("{}");
  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  doResponseCompression(
      {{":method", "get"}, {"content-length", "256"}, {"cache-control", "no-cache"}});
}

// Verifies isAcceptEncodingAllowed function.
TEST_F(CompressorFilterTest, isAcceptEncodingAllowed) {
  SetUpFilter("{}");
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, test, br"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(1, stats_.counter("test.test.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, test;q=1.0, *;q=0.5"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(2, stats_.counter("test.test.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {
        {"accept-encoding", "\tdeflate\t, test\t ; q\t =\t 1.0,\t * ;q=0.5"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(3, stats_.counter("test.test.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate,test;q=1.0,*;q=0"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(4, stats_.counter("test.test.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, test;q=0.2, br;q=1"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(5, stats_.counter("test.test.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "*"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(1, stats_.counter("test.test.header_wildcard").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "*;q=1"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(2, stats_.counter("test.test.header_wildcard").value());
  }
  {
    // test header is not valid due to q=0.
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "test;q=0,*;q=1"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(5, stats_.counter("test.test.header_compressor_used").value());
    EXPECT_EQ(1, stats_.counter("test.test.header_not_valid").value());
  }
  {
    Http::TestHeaderMapImpl headers = {};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(1, stats_.counter("test.test.no_accept_header").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity, *;q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(1, stats_.counter("test.test.header_identity").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity;q=0.5, *;q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(2, stats_.counter("test.test.header_identity").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity;q=0, *;q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(2, stats_.counter("test.test.header_identity").value());
    EXPECT_EQ(2, stats_.counter("test.test.header_not_valid").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "xyz;q=1, br;q=0.2, *"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(3, stats_.counter("test.test.header_wildcard").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "xyz;q=1, br;q=0.2, *;q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(3, stats_.counter("test.test.header_wildcard").value());
    EXPECT_EQ(3, stats_.counter("test.test.header_not_valid").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "xyz;q=1, br;q=0.2"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(4, stats_.counter("test.test.header_not_valid").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(3, stats_.counter("test.test.header_identity").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity;q=1"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(4, stats_.counter("test.test.header_identity").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity;q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(4, stats_.counter("test.test.header_identity").value());
    EXPECT_EQ(5, stats_.counter("test.test.header_not_valid").value());
  }
  {
    // Test that we return identity and ignore the invalid wildcard.
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity, *;q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(5, stats_.counter("test.test.header_identity").value());
    EXPECT_EQ(5, stats_.counter("test.test.header_not_valid").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, test;Q=.5, br"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(6, stats_.counter("test.test.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "identity;Q=0"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(5, stats_.counter("test.test.header_identity").value());
    EXPECT_EQ(6, stats_.counter("test.test.header_not_valid").value());
  }
}

} // namespace Compressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
