#include "common/buffer/buffer_impl.h"
#include "common/common/hex.h"
#include "common/compressor/brotli_compressor_impl.h"
#include "common/decompressor/brotli_decompressor_impl.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Decompressor {
namespace {

class BrotliDecompressorImplTest : public testing::Test {
protected:
  void drainBuffer(Buffer::OwnedImpl& buffer) { buffer.drain(buffer.length()); }

  void testcompressDecompressWithUncommonParams(
      uint32_t quality,
      uint32_t window_bits,
      uint32_t input_block_bits, bool disable_literal_context_modeling,
      Envoy::Compressor::BrotliCompressorImpl::EncoderMode encoder_mode) {
    Buffer::OwnedImpl buffer;
    Buffer::OwnedImpl accumulation_buffer;

    Envoy::Compressor::BrotliCompressorImpl compressor;
    compressor.init(quality, window_bits, input_block_bits, disable_literal_context_modeling, encoder_mode);

    std::string original_text{};
    for (uint64_t i = 0; i < 30; ++i) {
      TestUtility::feedBufferWithRandomCharacters(buffer, default_input_size * i, i);
      original_text.append(buffer.toString());
      compressor.compress(buffer, Compressor::State::Flush);
      accumulation_buffer.add(buffer);
      drainBuffer(buffer);
    }
    ASSERT_EQ(0, buffer.length());

    compressor.compress(buffer, Compressor::State::Finish);
    accumulation_buffer.add(buffer);

    drainBuffer(buffer);
    ASSERT_EQ(0, buffer.length());

    BrotliDecompressorImpl decompressor;
    decompressor.init();

    decompressor.decompress(accumulation_buffer, buffer);
    std::string decompressed_text{buffer.toString()};

    //ASSERT_EQ(compressor.checksum(), decompressor.checksum());
    ASSERT_EQ(original_text.length(), decompressed_text.length());
    EXPECT_EQ(original_text, decompressed_text);
  }

  static const uint32_t brotli_window_bits{24};
  static const uint32_t brotli_input_block_bits{24};
  static const uint32_t default_input_size{796};
};

class BrotliDecompressorImplDeathTest : public BrotliDecompressorImplTest {
protected:
  static void decompressorBadInitTestHelper() {
    BrotliDecompressorImpl decompressor;
    decompressor.init();
  }

  static void unitializedDecompressorTestHelper() {
    Buffer::OwnedImpl input_buffer;
    Buffer::OwnedImpl output_buffer;
    BrotliDecompressorImpl decompressor;
    TestUtility::feedBufferWithRandomCharacters(input_buffer, 100);
    decompressor.decompress(input_buffer, output_buffer);
  }
};

// Exercises death by passing bad initialization params or by calling decompress before init.
TEST_F(BrotliDecompressorImplDeathTest, DecompressorDeathTest) {
  EXPECT_DEATH_LOG_TO_STDERR(decompressorBadInitTestHelper(), "assert failure: result >= 0");
  EXPECT_DEATH_LOG_TO_STDERR(unitializedDecompressorTestHelper(), "assert failure: result == Z_OK");
}

// Exercises compression and decompression by compressing some data, decompressing it and then
// comparing compressor's input/checksum with decompressor's output/checksum.
TEST_F(BrotliDecompressorImplTest, CompressAndDecompress) {
  Buffer::OwnedImpl buffer;
  Buffer::OwnedImpl accumulation_buffer;

  Envoy::Compressor::BrotliCompressorImpl compressor;
  compressor.init(11, 24, 24, false, Envoy::Compressor::BrotliCompressorImpl::EncoderMode::Default);

  std::string original_text{};
  for (uint64_t i = 0; i < 20; ++i) {
    TestUtility::feedBufferWithRandomCharacters(buffer, default_input_size * i, i);
    original_text.append(buffer.toString());
    compressor.compress(buffer, Compressor::State::Flush);
    accumulation_buffer.add(buffer);
    drainBuffer(buffer);
  }

  ASSERT_EQ(0, buffer.length());

  compressor.compress(buffer, Compressor::State::Finish);
  ASSERT_GE(10, buffer.length());

  accumulation_buffer.add(buffer);

  drainBuffer(buffer);
  ASSERT_EQ(0, buffer.length());

  BrotliDecompressorImpl decompressor;
  decompressor.init();

  decompressor.decompress(accumulation_buffer, buffer);
  std::string decompressed_text{buffer.toString()};

  ASSERT_EQ(original_text.length(), decompressed_text.length());
  EXPECT_EQ(original_text, decompressed_text);
}

// Exercises decompression with a very small output buffer.
TEST_F(BrotliDecompressorImplTest, DecompressWithSmallOutputBuffer) {
  Buffer::OwnedImpl buffer;
  Buffer::OwnedImpl accumulation_buffer;

  Envoy::Compressor::BrotliCompressorImpl compressor;
  compressor.init(11, 24, 24, false, Envoy::Compressor::BrotliCompressorImpl::EncoderMode::Default);

  std::string original_text{};
  for (uint64_t i = 0; i < 20; ++i) {
    TestUtility::feedBufferWithRandomCharacters(buffer, default_input_size * i, i);
    original_text.append(buffer.toString());
    compressor.compress(buffer, Compressor::State::Flush);
    accumulation_buffer.add(buffer);
    drainBuffer(buffer);
  }

  ASSERT_EQ(0, buffer.length());

  compressor.compress(buffer, Compressor::State::Finish);
  ASSERT_GE(10, buffer.length());

  accumulation_buffer.add(buffer);

  drainBuffer(buffer);
  ASSERT_EQ(0, buffer.length());

  BrotliDecompressorImpl decompressor(16);
  decompressor.init();

  decompressor.decompress(accumulation_buffer, buffer);
  std::string decompressed_text{buffer.toString()};

  ASSERT_EQ(original_text.length(), decompressed_text.length());
  EXPECT_EQ(original_text, decompressed_text);
}

// Exercises decompression with other supported brotli initialization params.
TEST_F(BrotliDecompressorImplTest, CompressDecompressWithUncommonParams) {
  // Test with different memory levels.
  for (uint32_t i = 1; i < 10; ++i) {
    testcompressDecompressWithUncommonParams(
      i-1, // quality
      15, // window_bits
      15, // input_block_bits
      false, // disable_literal_context_modeling
      Envoy::Compressor::BrotliCompressorImpl::EncoderMode::Font);

    testcompressDecompressWithUncommonParams(
      11, // quality
      15, // window_bits
      i + 10, // input_block_bits
      false, // disable_literal_context_modeling
      Envoy::Compressor::BrotliCompressorImpl::EncoderMode::Text);

    testcompressDecompressWithUncommonParams(
      11, // quality
      i + 10, // window_bits
      15, // input_block_bits
      false, // disable_literal_context_modeling
      Envoy::Compressor::BrotliCompressorImpl::EncoderMode::Generic);
  }
}

TEST_F(BrotliDecompressorImplTest, CompressDecompressOfMultipleSlices) {
  Buffer::OwnedImpl buffer;
  Buffer::OwnedImpl accumulation_buffer;

  const std::string sample{"slice, slice, slice, slice, slice, "};
  std::string original_text;
  for (uint64_t i = 0; i < 20; ++i) {
    Buffer::BufferFragmentImpl* frag = new Buffer::BufferFragmentImpl(
        sample.c_str(), sample.size(),
        [](const void*, size_t, const Buffer::BufferFragmentImpl* frag) { delete frag; });

    buffer.addBufferFragment(*frag);
    original_text.append(sample);
  }

  const uint64_t num_slices = buffer.getRawSlices(nullptr, 0);
  EXPECT_EQ(num_slices, 20);

  Envoy::Compressor::BrotliCompressorImpl compressor;
  compressor.init(11, 24, 24, false, Envoy::Compressor::BrotliCompressorImpl::EncoderMode::Default);

  compressor.compress(buffer, Compressor::State::Flush);
  accumulation_buffer.add(buffer);

  BrotliDecompressorImpl decompressor;
  decompressor.init();

  drainBuffer(buffer);
  ASSERT_EQ(0, buffer.length());

  decompressor.decompress(accumulation_buffer, buffer);
  std::string decompressed_text{buffer.toString()};

  ASSERT_EQ(original_text.length(), decompressed_text.length());
  EXPECT_EQ(original_text, decompressed_text);
}

} // namespace
} // namespace Decompressor
} // namespace Envoy
