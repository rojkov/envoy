#include "envoy/extensions/filters/http/compressor/v3alpha/compressor.pb.h"

#include "common/protobuf/utility.h"

#include "extensions/filters/http/common/compressor/compressor.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/protobuf/mocks.h"
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
  void compress(Buffer::Instance&, ::Envoy::Compressor::State) override {}
};

class MockCompressorFilterConfig : public CompressorFilterConfig {
public:
  MockCompressorFilterConfig(
      const envoy::extensions::filters::http::compressor::v3alpha::Compressor& compressor,
      const std::string& stats_prefix, Stats::Scope& scope, Runtime::Loader& runtime,
      const std::string& compressor_name)
      : CompressorFilterConfig(compressor, stats_prefix + compressor_name + ".", scope, runtime,
                               compressor_name) {}

  std::unique_ptr<Compressor::Compressor> makeCompressor() override {
    return std::make_unique<MockCompressor>();
  }
  const std::string featureName() const override { return "test.filter_enabled"; }
};

class CompressorFilterTest : public testing::Test {
protected:
  CompressorFilterTest() {
    ON_CALL(runtime_.snapshot_, featureEnabled("test.filter_enabled", 100))
        .WillByDefault(Return(true));
  }

  void SetUp() override { setUpFilter("{}"); }

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

  bool isAcceptEncodingAllowed(Http::HeaderMap& headers,
                               const std::unique_ptr<CompressorFilter>& filter = nullptr) {
    if (filter) {
      return filter->isAcceptEncodingAllowed(headers);
    } else {
      NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
      filter_->setDecoderFilterCallbacks(decoder_callbacks);
      return filter_->isAcceptEncodingAllowed(headers);
    }
  }

  bool isMinimumContentLength(Http::HeaderMap& headers) {
    return filter_->isMinimumContentLength(headers);
  }

  bool isTransferEncodingAllowed(Http::HeaderMap& headers) {
    return filter_->isTransferEncodingAllowed(headers);
  }

  // CompressorFilterTest Helpers
  void setUpFilter(std::string&& json) {
    envoy::extensions::filters::http::compressor::v3alpha::Compressor compressor;
    TestUtility::loadFromJson(json, compressor);
    config_.reset(new MockCompressorFilterConfig(compressor, "test.", stats_, runtime_, "test"));
    filter_ = std::make_unique<CompressorFilter>(config_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
  }

  void verifyCompressedData() {
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
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
    filter_->setDecoderFilterCallbacks(decoder_callbacks);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, end_stream));
  }

  void doResponseCompression(Http::TestHeaderMapImpl&& headers, bool with_trailers) {
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
      EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->encodeTrailers(headers));
    }
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
  NiceMock<Http::MockStreamEncoderFilterCallbacks> encoder_callbacks_;
};

// Test if Runtime Feature is Disabled
TEST_F(CompressorFilterTest, DecodeHeadersWithRuntimeDisabled) {
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("test.filter_enabled", 100))
      .WillOnce(Return(false));

  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  doResponseNoCompression({{":method", "get"}, {"content-length", "256"}});
}

// Default config values.
TEST_F(CompressorFilterTest, DefaultConfigValues) {
  EXPECT_EQ(30, config_->minimumLength());
  EXPECT_EQ(false, config_->disableOnEtagHeader());
  EXPECT_EQ(false, config_->removeAcceptEncodingHeader());
  EXPECT_EQ(8, config_->contentTypeValues().size());
}

// Acceptance Testing with default configuration.
TEST_F(CompressorFilterTest, AcceptanceTestEncoding) {
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestHeaderMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));
  doResponseCompression({{":method", "get"}, {"content-length", "256"}}, false);
}

TEST_F(CompressorFilterTest, AcceptanceTestEncodingWithTrailers) {
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, test"}}, false);
  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestHeaderMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));
  doResponseCompression({{":method", "get"}, {"content-length", "256"}}, true);
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
  doRequest({{":method", "get"}, {"accept-encoding", "test;q=0, deflate"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"cache-control", "no-transform"}});
}

// Verifies that compression is NOT skipped when cache-control header does NOT have no-transform
// value.
TEST_F(CompressorFilterTest, hasCacheControlNoTransformCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  doResponseCompression(
      {{":method", "get"}, {"content-length", "256"}, {"cache-control", "no-cache"}}, false);
}

// Verifies isAcceptEncodingAllowed function.
TEST_F(CompressorFilterTest, isAcceptEncodingAllowed) {
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
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", ""}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(5, stats_.counter("test.test.header_identity").value());
    EXPECT_EQ(7, stats_.counter("test.test.header_not_valid").value());
  }
  {
    // Compressor "test2" from an independent filter chain should not overshadow "test".
    // The independence is simulated with a new instance DecoderFilterCallbacks set for "test2".
    Stats::IsolatedStoreImpl stats;
    NiceMock<Runtime::MockLoader> runtime;
    envoy::extensions::filters::http::compressor::v3alpha::Compressor compressor;
    TestUtility::loadFromJson("{}", compressor);
    CompressorFilterConfigSharedPtr config2;
    config2.reset(new MockCompressorFilterConfig(compressor, "test2.", stats, runtime, "test2"));
    std::unique_ptr<CompressorFilter> filter2 = std::make_unique<CompressorFilter>(config2);
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
    filter2->setDecoderFilterCallbacks(decoder_callbacks);

    Http::TestHeaderMapImpl headers = {{"accept-encoding", "test;Q=.5,test2;q=0.75"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers));
    EXPECT_TRUE(isAcceptEncodingAllowed(headers, filter2));
    EXPECT_EQ(0, stats_.counter("test.test.header_compressor_overshadowed").value());
    EXPECT_EQ(7, stats_.counter("test.test.header_compressor_used").value());
    EXPECT_EQ(1, stats.counter("test2.test2.header_compressor_used").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "test;q=invalid"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers));
    EXPECT_EQ(8, stats_.counter("test.test.header_not_valid").value());
  }
  {
    // check if the legacy "header_gzip" counter is incremented for gzip compression filter
    Stats::IsolatedStoreImpl stats;
    NiceMock<Runtime::MockLoader> runtime;
    envoy::extensions::filters::http::compressor::v3alpha::Compressor compressor;
    TestUtility::loadFromJson("{}", compressor);
    CompressorFilterConfigSharedPtr config2;
    config2.reset(new MockCompressorFilterConfig(compressor, "test2.", stats, runtime, "gzip"));
    std::unique_ptr<CompressorFilter> gzip_filter = std::make_unique<CompressorFilter>(config2);
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
    gzip_filter->setDecoderFilterCallbacks(decoder_callbacks);

    Http::TestHeaderMapImpl headers = {{"accept-encoding", "gzip;q=0.75"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers, gzip_filter));
    EXPECT_EQ(1, stats.counter("test2.gzip.header_gzip").value());
  }
  {
    // Test that encoding decision is cached when used by multiple filters.
    Stats::IsolatedStoreImpl stats;
    NiceMock<Runtime::MockLoader> runtime;
    envoy::extensions::filters::http::compressor::v3alpha::Compressor compressor;
    TestUtility::loadFromJson("{}", compressor);
    CompressorFilterConfigSharedPtr config1;
    config1.reset(new MockCompressorFilterConfig(compressor, "test1.", stats, runtime, "test1"));
    std::unique_ptr<CompressorFilter> filter1 = std::make_unique<CompressorFilter>(config1);
    CompressorFilterConfigSharedPtr config2;
    config2.reset(new MockCompressorFilterConfig(compressor, "test2.", stats, runtime, "test2"));
    std::unique_ptr<CompressorFilter> filter2 = std::make_unique<CompressorFilter>(config2);
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
    filter1->setDecoderFilterCallbacks(decoder_callbacks);
    filter2->setDecoderFilterCallbacks(decoder_callbacks);

    Http::TestHeaderMapImpl headers = {{"accept-encoding", "test1;Q=.5,test2;q=0.75"}};
    EXPECT_FALSE(isAcceptEncodingAllowed(headers, filter1));
    EXPECT_TRUE(isAcceptEncodingAllowed(headers, filter2));
    EXPECT_EQ(1, stats.counter("test1.test1.header_compressor_overshadowed").value());
    EXPECT_EQ(1, stats.counter("test2.test2.header_compressor_used").value());
    EXPECT_FALSE(isAcceptEncodingAllowed(headers, filter1));
    EXPECT_EQ(2, stats.counter("test1.test1.header_compressor_overshadowed").value());
    // These headers are ignored. Instead the cached decision is used.
    Http::TestHeaderMapImpl headers2 = {{"accept-encoding", "fake"}};
    EXPECT_TRUE(isAcceptEncodingAllowed(headers2, filter2));
    EXPECT_EQ(2, stats.counter("test2.test2.header_compressor_used").value());
  }
}

// Verifies that compression is skipped when accept-encoding header is not allowed.
TEST_F(CompressorFilterTest, AcceptEncodingNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test;q=0, deflate"}}, true);
  doResponseNoCompression({{":method", "get"}, {"content-length", "256"}});
}

// Verifies that compression is NOT skipped when accept-encoding header is allowed.
TEST_F(CompressorFilterTest, AcceptEncodingCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test, deflate"}}, true);
  doResponseCompression({{":method", "get"}, {"content-length", "256"}}, false);
}

// Verifies isMinimumContentLength function.
TEST_F(CompressorFilterTest, isMinimumContentLength) {
  {
    Http::TestHeaderMapImpl headers = {{"content-length", "31"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-length", "29"}};
    EXPECT_FALSE(isMinimumContentLength(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "chunked"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "Chunked"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }

  setUpFilter(R"EOF({"content_length": 500})EOF");
  {
    Http::TestHeaderMapImpl headers = {{"content-length", "501"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "chunked"}};
    EXPECT_TRUE(isMinimumContentLength(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-length", "499"}};
    EXPECT_FALSE(isMinimumContentLength(headers));
  }
}

// Verifies that compression is skipped when content-length header is NOT allowed.
TEST_F(CompressorFilterTest, ContentLengthNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseNoCompression({{":method", "get"}, {"content-length", "10"}});
}

// Verifies that compression is NOT skipped when content-length header is allowed.
TEST_F(CompressorFilterTest, ContentLengthCompression) {
  setUpFilter(R"EOF({"content_length": 500})EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseCompression({{":method", "get"}, {"content-length", "1000"}}, false);
}

// Verifies isContentTypeAllowed function.
TEST_F(CompressorFilterTest, isContentTypeAllowed) {

  {
    Http::TestHeaderMapImpl headers = {{"content-type", "text/html"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "text/xml"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "text/plain"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "application/javascript"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "image/svg+xml"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "application/json;charset=utf-8"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "application/json"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "application/xhtml+xml"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "Application/XHTML+XML"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "image/jpeg"}};
    EXPECT_FALSE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "\ttext/html\t"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }

  setUpFilter(R"EOF(
    {
      "content_type": [
        "text/html",
        "xyz/svg+xml",
        "Test/INSENSITIVE"
      ]
    }
  )EOF");

  {
    Http::TestHeaderMapImpl headers = {{"content-type", "xyz/svg+xml"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "xyz/false"}};
    EXPECT_FALSE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "image/jpeg"}};
    EXPECT_FALSE(isContentTypeAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"content-type", "test/insensitive"}};
    EXPECT_TRUE(isContentTypeAllowed(headers));
  }
}

// Verifies that compression is skipped when content-encoding header is NOT allowed.
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
      ]
    }
  )EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"content-type", "image/jpeg"}});
}

// Verifies that compression is NOT skipped when content-encoding header is allowed.
TEST_F(CompressorFilterTest, ContentTypeCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseCompression({{":method", "get"},
                         {"content-length", "256"},
                         {"content-type", "application/json;charset=utf-8"}},
                        false);
}

// Verifies sanitizeEtagHeader function.
TEST_F(CompressorFilterTest, sanitizeEtagHeader) {
  {
    std::string etag_header{R"EOF(W/"686897696a7c876b7e")EOF"};
    Http::TestHeaderMapImpl headers = {{"etag", etag_header}};
    sanitizeEtagHeader(headers);
    EXPECT_EQ(etag_header, headers.get_("etag"));
  }
  {
    std::string etag_header{R"EOF(w/"686897696a7c876b7e")EOF"};
    Http::TestHeaderMapImpl headers = {{"etag", etag_header}};
    sanitizeEtagHeader(headers);
    EXPECT_EQ(etag_header, headers.get_("etag"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"etag", "686897696a7c876b7e"}};
    sanitizeEtagHeader(headers);
    EXPECT_FALSE(headers.has("etag"));
  }
}

// Verifies isEtagAllowed function.
TEST_F(CompressorFilterTest, isEtagAllowed) {
  {
    Http::TestHeaderMapImpl headers = {{"etag", R"EOF(W/"686897696a7c876b7e")EOF"}};
    EXPECT_TRUE(isEtagAllowed(headers));
    EXPECT_EQ(0, stats_.counter("test.test.not_compressed_etag").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"etag", "686897696a7c876b7e"}};
    EXPECT_TRUE(isEtagAllowed(headers));
    EXPECT_EQ(0, stats_.counter("test.test.not_compressed_etag").value());
  }
  {
    Http::TestHeaderMapImpl headers = {};
    EXPECT_TRUE(isEtagAllowed(headers));
    EXPECT_EQ(0, stats_.counter("test.test.not_compressed_etag").value());
  }

  setUpFilter(R"EOF({ "disable_on_etag_header": true })EOF");
  {
    Http::TestHeaderMapImpl headers = {{"etag", R"EOF(W/"686897696a7c876b7e")EOF"}};
    EXPECT_FALSE(isEtagAllowed(headers));
    EXPECT_EQ(1, stats_.counter("test.test.not_compressed_etag").value());
  }
  {
    Http::TestHeaderMapImpl headers = {{"etag", "686897696a7c876b7e"}};
    EXPECT_FALSE(isEtagAllowed(headers));
    EXPECT_EQ(2, stats_.counter("test.test.not_compressed_etag").value());
  }
  {
    Http::TestHeaderMapImpl headers = {};
    EXPECT_TRUE(isEtagAllowed(headers));
    EXPECT_EQ(2, stats_.counter("test.test.not_compressed_etag").value());
  }
}

// Verifies that compression is skipped when etag header is NOT allowed.
TEST_F(CompressorFilterTest, EtagNoCompression) {
  setUpFilter(R"EOF({ "disable_on_etag_header": true })EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"etag", R"EOF(W/"686897696a7c876b7e")EOF"}});
  EXPECT_EQ(1, stats_.counter("test.test.not_compressed_etag").value());
}

// Verifies that compression is skipped when etag header is NOT allowed.
TEST_F(CompressorFilterTest, EtagCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"etag", "686897696a7c876b7e"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_FALSE(headers.has("etag"));
  EXPECT_EQ("test", headers.get_("content-encoding"));
}

// Verifies isTransferEncodingAllowed function.
TEST_F(CompressorFilterTest, isTransferEncodingAllowed) {
  {
    Http::TestHeaderMapImpl headers = {};
    EXPECT_TRUE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "chunked"}};
    EXPECT_TRUE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "Chunked"}};
    EXPECT_TRUE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "deflate"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "Deflate"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "test"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", "test, chunked"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
  {
    Http::TestHeaderMapImpl headers = {{"transfer-encoding", " test\t,  chunked\t"}};
    EXPECT_FALSE(isTransferEncodingAllowed(headers));
  }
}

// Tests compression when Transfer-Encoding header exists.
TEST_F(CompressorFilterTest, TransferEncodingChunked) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseCompression(
      {{":method", "get"}, {"content-length", "256"}, {"transfer-encoding", "chunked"}}, false);
}

// Tests compression when Transfer-Encoding header exists.
TEST_F(CompressorFilterTest, AcceptanceTransferEncoding) {

  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"transfer-encoding", "chunked, deflate"}});
}

// Content-Encoding: upstream response is already encoded.
TEST_F(CompressorFilterTest, ContentEncodingAlreadyEncoded) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestHeaderMapImpl response_headers{
      {":method", "get"}, {"content-length", "256"}, {"content-encoding", "deflate, gzip"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
  EXPECT_TRUE(response_headers.has("content-length"));
  EXPECT_FALSE(response_headers.has("transfer-encoding"));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, false));
}

// No compression when upstream response is empty.
TEST_F(CompressorFilterTest, EmptyResponse) {

  Http::TestHeaderMapImpl headers{{":method", "get"}, {":status", "204"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, true));
  EXPECT_EQ("", headers.get_("content-length"));
  EXPECT_EQ("", headers.get_("content-encoding"));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, true));
}

// Verifies insertVaryHeader function.
TEST_F(CompressorFilterTest, insertVaryHeader) {
  {
    Http::TestHeaderMapImpl headers = {};
    insertVaryHeader(headers);
    EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"vary", "Cookie"}};
    insertVaryHeader(headers);
    EXPECT_EQ("Cookie, Accept-Encoding", headers.get_("vary"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"vary", "accept-encoding"}};
    insertVaryHeader(headers);
    EXPECT_EQ("accept-encoding, Accept-Encoding", headers.get_("vary"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"vary", "Accept-Encoding, Cookie"}};
    insertVaryHeader(headers);
    EXPECT_EQ("Accept-Encoding, Cookie", headers.get_("vary"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"vary", "Accept-Encoding"}};
    insertVaryHeader(headers);
    EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
  }
}

// Filter should set Vary header value with `accept-encoding`.
TEST_F(CompressorFilterTest, NoVaryHeader) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
}

// Filter should set Vary header value with `accept-encoding` and preserve other values.
TEST_F(CompressorFilterTest, VaryOtherValues) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"vary", "User-Agent, Cookie"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("User-Agent, Cookie, Accept-Encoding", headers.get_("vary"));
}

// Vary header should have only one `accept-encoding`value.
TEST_F(CompressorFilterTest, VaryAlreadyHasAcceptEncoding) {
  doRequest({{":method", "get"}, {"accept-encoding", "test"}}, true);
  Http::TestHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"vary", "accept-encoding"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("accept-encoding, Accept-Encoding", headers.get_("vary"));
}

// Verify removeAcceptEncoding header.
TEST_F(CompressorFilterTest, RemoveAcceptEncodingHeader) {
  NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks;
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, test, gzip, br"}};
    setUpFilter(R"EOF({"remove_accept_encoding_header": true})EOF");
    filter_->setDecoderFilterCallbacks(decoder_callbacks);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_FALSE(headers.has("accept-encoding"));
  }
  {
    NiceMock<Http::MockStreamDecoderFilterCallbacks> decoder_callbacks2;
    filter_->setDecoderFilterCallbacks(decoder_callbacks2);
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, gzip, br"}};
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_TRUE(headers.has("accept-encoding"));
    EXPECT_EQ("deflate, gzip, br", headers.get_("accept-encoding"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, test, gzip, br"}};
    setUpFilter("{}");
    filter_->setDecoderFilterCallbacks(decoder_callbacks);
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_TRUE(headers.has("accept-encoding"));
    EXPECT_EQ("deflate, test, gzip, br", headers.get_("accept-encoding"));
  }
}

} // namespace Compressors
} // namespace Common
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
