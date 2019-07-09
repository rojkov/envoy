#include <memory>

#include "common/compressor/brotli_compressor_impl.h"
#include "common/decompressor/brotli_decompressor_impl.h"
#include "common/protobuf/utility.h"

#include "extensions/filters/http/brotli/brotli_filter.h"

#include "test/mocks/http/mocks.h"
#include "test/mocks/runtime/mocks.h"
#include "test/mocks/stats/mocks.h"
#include "test/test_common/utility.h"

#include "gtest/gtest.h"

using testing::Return;

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Brotli {

class BrotliFilterTest : public testing::Test {
protected:
  BrotliFilterTest() {
    ON_CALL(runtime_.snapshot_, featureEnabled("brotli.filter_enabled", 100))
        .WillByDefault(Return(true));
  }

  void SetUp() override {
    setUpFilter("{}");
    decompressor_.init(false);
  }

  // BrotliFilterTest Helpers
  void setUpFilter(std::string&& json) {
    envoy::config::filter::http::brotli::v2::Brotli brotli;
    TestUtility::loadFromJson(json, brotli);
    config_.reset(new BrotliFilterConfig(brotli, "test.", stats_, runtime_));
    filter_ = std::make_unique<Common::Compressors::CompressorFilter>(config_);
  }

  void verifyCompressedData() {
    decompressor_.decompress(data_, decompressed_data_);
    const std::string uncompressed_str{decompressed_data_.toString()};
    ASSERT_EQ(expected_str_.length(), uncompressed_str.length());
    EXPECT_EQ(expected_str_, uncompressed_str);
    EXPECT_EQ(expected_str_.length(),
              stats_.counter("test.brotli.total_uncompressed_bytes").value());
    EXPECT_EQ(data_.length(), stats_.counter("test.brotli.total_compressed_bytes").value());
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
    EXPECT_EQ(Http::Headers::get().ContentEncodingValues.Brotli, headers.get_("content-encoding"));
    EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, true));
    verifyCompressedData();
    drainBuffer();
    EXPECT_EQ(1U, stats_.counter("test.brotli.compressed").value());
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
    EXPECT_EQ(1, stats_.counter("test.brotli.not_compressed").value());
  }

  std::shared_ptr<BrotliFilterConfig> config_;
  std::unique_ptr<Common::Compressors::CompressorFilter> filter_;
  Buffer::OwnedImpl data_;
  Decompressor::BrotliDecompressorImpl decompressor_;
  Buffer::OwnedImpl decompressed_data_;
  std::string expected_str_;
  Stats::IsolatedStoreImpl stats_;
  NiceMock<Runtime::MockLoader> runtime_;
};

// Test if Runtime Feature is Disabled
TEST_F(BrotliFilterTest, RuntimeDisabled) {
  EXPECT_CALL(runtime_.snapshot_, featureEnabled("brotli.filter_enabled", 100))
      .WillOnce(Return(false));
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, br"}}, false);
  doResponseNoCompression({{":method", "get"}, {"content-length", "256"}});
}

// Default config values.
TEST_F(BrotliFilterTest, DefaultConfigValues) {
  EXPECT_EQ(11, config_->quality());
  EXPECT_EQ(30, config_->minimumLength());
  EXPECT_EQ(24, config_->windowBits());
  EXPECT_EQ(false, config_->disableOnEtagHeader());
  EXPECT_EQ(false, config_->removeAcceptEncodingHeader());
  EXPECT_EQ(false, config_->disableLiteralContextModeling());
  EXPECT_EQ(Compressor::BrotliCompressorImpl::EncoderMode::Default, config_->encoderMode());
  EXPECT_EQ(8, config_->contentTypeValues().size());
}

// Acceptance Testing with default configuration.
TEST_F(BrotliFilterTest, AcceptanceBrotliEncoding) {
  doRequest({{":method", "get"}, {"accept-encoding", "deflate, br"}}, false);
  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->decodeData(data, false));
  Http::TestHeaderMapImpl trailers;
  EXPECT_EQ(Http::FilterTrailersStatus::Continue, filter_->decodeTrailers(trailers));
  doResponseCompression({{":method", "get"}, {"content-length", "256"}});
}

// Verifies that compression is skipped when cache-control header has no-transform value.
TEST_F(BrotliFilterTest, hasCacheControlNoTransformNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br;q=0, deflate"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"cache-control", "no-transform"}});
}

// Verifies that compression is NOT skipped when cache-control header does NOT have no-transform
// value.
TEST_F(BrotliFilterTest, hasCacheControlNoTransformCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br, deflate"}}, true);
  doResponseCompression(
      {{":method", "get"}, {"content-length", "256"}, {"cache-control", "no-cache"}});
}

// Verifies that compression is skipped when accept-encoding header is not allowed.
TEST_F(BrotliFilterTest, AcceptEncodingNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br;q=0, deflate"}}, true);
  doResponseNoCompression({{":method", "get"}, {"content-length", "256"}});
}

// Verifies that compression is NOT skipped when accept-encoding header is allowed.
TEST_F(BrotliFilterTest, AcceptEncodingCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br, deflate"}}, true);
  doResponseCompression({{":method", "get"}, {"content-length", "256"}});
}

// Verifies that compression is skipped when content-length header is NOT allowed.
TEST_F(BrotliFilterTest, ContentLengthNoCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseNoCompression({{":method", "get"}, {"content-length", "10"}});
}

// Verifies that compression is NOT skipped when content-length header is allowed.
TEST_F(BrotliFilterTest, ContentLengthCompression) {
  setUpFilter(R"EOF({"compressor": {"content_length": 500}})EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseCompression({{":method", "get"}, {"content-length", "1000"}});
}

// Verifies that compression is skipped when content-encoding header is NOT allowed.
TEST_F(BrotliFilterTest, ContentTypeNoCompression) {
  setUpFilter(R"EOF(
    {"compressor": {
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
    }
  )EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"content-type", "image/jpeg"}});
}

// Verifies that compression is NOT skipped when content-encoding header is allowed.
TEST_F(BrotliFilterTest, ContentTypeCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseCompression({{":method", "get"},
                         {"content-length", "256"},
                         {"content-type", "application/json;charset=utf-8"}});
}

// Verifies that compression is skipped when etag header is NOT allowed.
TEST_F(BrotliFilterTest, EtagNoCompression) {
  setUpFilter(R"EOF({"compressor": { "disable_on_etag_header": true }})EOF");
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"etag", R"EOF(W/"686897696a7c876b7e")EOF"}});
  EXPECT_EQ(1, stats_.counter("test.brotli.not_compressed_etag").value());
}

// Verifies that compression is skipped when etag header is NOT allowed.
TEST_F(BrotliFilterTest, EtagCompression) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  Http::TestHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"etag", "686897696a7c876b7e"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_FALSE(headers.has("etag"));
  EXPECT_EQ("br", headers.get_("content-encoding"));
}

// Tests compression when Transfer-Encoding header exists.
TEST_F(BrotliFilterTest, TransferEncodingChunked) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseCompression(
      {{":method", "get"}, {"content-length", "256"}, {"transfer-encoding", "chunked"}});
}

// Tests compression when Transfer-Encoding header exists.
TEST_F(BrotliFilterTest, AcceptanceTransferEncodingBrotli) {

  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  doResponseNoCompression(
      {{":method", "get"}, {"content-length", "256"}, {"transfer-encoding", "chunked, deflate"}});
}

// Content-Encoding: upstream response is already encoded.
TEST_F(BrotliFilterTest, ContentEncodingAlreadyEncoded) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  Http::TestHeaderMapImpl response_headers{
      {":method", "get"}, {"content-length", "256"}, {"content-encoding", "deflate, br"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(response_headers, false));
  EXPECT_TRUE(response_headers.has("content-length"));
  EXPECT_FALSE(response_headers.has("transfer-encoding"));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, false));
}

// No compression when upstream response is empty.
TEST_F(BrotliFilterTest, EmptyResponse) {

  Http::TestHeaderMapImpl headers{{":method", "get"}, {":status", "204"}};
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, true));
  EXPECT_EQ("", headers.get_("content-length"));
  EXPECT_EQ("", headers.get_("content-encoding"));
  EXPECT_EQ(Http::FilterDataStatus::Continue, filter_->encodeData(data_, true));
}

// Filter should set Vary header value with `accept-encoding`.
TEST_F(BrotliFilterTest, NoVaryHeader) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  Http::TestHeaderMapImpl headers{{":method", "get"}, {"content-length", "256"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("Accept-Encoding", headers.get_("vary"));
}

// Filter should set Vary header value with `accept-encoding` and preserve other values.
TEST_F(BrotliFilterTest, VaryOtherValues) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  Http::TestHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"vary", "User-Agent, Cookie"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("User-Agent, Cookie, Accept-Encoding", headers.get_("vary"));
}

// Vary header should have only one `accept-encoding`value.
TEST_F(BrotliFilterTest, VaryAlreadyHasAcceptEncoding) {
  doRequest({{":method", "get"}, {"accept-encoding", "br"}}, true);
  Http::TestHeaderMapImpl headers{
      {":method", "get"}, {"content-length", "256"}, {"vary", "accept-encoding"}};
  feedBuffer(256);
  EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->encodeHeaders(headers, false));
  EXPECT_TRUE(headers.has("vary"));
  EXPECT_EQ("accept-encoding, Accept-Encoding", headers.get_("vary"));
}

// Verify removeAcceptEncoding header.
TEST_F(BrotliFilterTest, RemoveAcceptEncodingHeader) {
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, br, gzip"}};
    setUpFilter(R"EOF({"compressor": {"remove_accept_encoding_header": true}})EOF");
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_FALSE(headers.has("accept-encoding"));
  }
  {
    Http::TestHeaderMapImpl headers = {{"accept-encoding", "deflate, br, gzip"}};
    setUpFilter("{}");
    EXPECT_EQ(Http::FilterHeadersStatus::Continue, filter_->decodeHeaders(headers, true));
    EXPECT_TRUE(headers.has("accept-encoding"));
    EXPECT_EQ("deflate, br, gzip", headers.get_("accept-encoding"));
  }
}

} // namespace Brotli
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
