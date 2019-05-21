#include "common/buffer/buffer_impl.h"
#include "common/common/stack_array.h"
#include "common/compressor/brotli_compressor_impl.h"
#include "common/decompressor/brotli_decompressor_impl.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Compressor {
namespace {

class BrotliCompressorImplTest : public testing::Test {
protected:
  void drainBuffer(Buffer::OwnedImpl& buffer) { buffer.drain(buffer.length()); }

  static const uint32_t default_quality{11};
  static const uint32_t default_window_bits{22};
  static const uint32_t default_input_block_bits{22};
  static const uint64_t default_input_size{796};
};

TEST_F(BrotliCompressorImplTest, CompressorDeathTest) {
  BrotliCompressorImpl compressor;
  EXPECT_DEATH_LOG_TO_STDERR(compressor.init(1000, default_window_bits, default_input_block_bits,
                                             false, BrotliCompressorImpl::EncoderMode::Generic),
                             "assert failure: quality <= BROTLI_MAX_QUALITY");
  EXPECT_DEATH_LOG_TO_STDERR(
      compressor.init(default_quality, 1, default_input_block_bits, false,
                      BrotliCompressorImpl::EncoderMode::Generic),
      "assert failure: window_bits >= BROTLI_MIN_WINDOW_BITS && window_bits <= "
      "BROTLI_MAX_WINDOW_BITS");
  EXPECT_DEATH_LOG_TO_STDERR(
      compressor.init(default_quality, default_window_bits, 30, false,
                      BrotliCompressorImpl::EncoderMode::Generic),
      "assert failure: input_block_bits >= BROTLI_MIN_INPUT_BLOCK_BITS && input_block_bits <= "
      "BROTLI_MAX_INPUT_BLOCK_BITS");
}

TEST_F(BrotliCompressorImplTest, CallingFinishOnly) {
  Buffer::OwnedImpl buffer;
  BrotliCompressorImpl compressor;

  compressor.init(default_quality, default_window_bits, default_input_block_bits, false,
                  BrotliCompressorImpl::EncoderMode::Default);
  TestUtility::feedBufferWithRandomCharacters(buffer, 4096);
  compressor.compress(buffer, State::Finish);
}

TEST_F(BrotliCompressorImplTest, CallingFlushOnly) {
  Buffer::OwnedImpl buffer;
  BrotliCompressorImpl compressor;

  compressor.init(default_quality, default_window_bits, default_input_block_bits, false,
                  BrotliCompressorImpl::EncoderMode::Default);
  TestUtility::feedBufferWithRandomCharacters(buffer, 4096);
  compressor.compress(buffer, State::Flush);
}

TEST_F(BrotliCompressorImplTest, CompressWithSmallChunkSize) {
  Buffer::OwnedImpl buffer;
  Buffer::OwnedImpl accumulation_buffer;
  BrotliCompressorImpl compressor(8);

  compressor.init(default_quality, default_window_bits, default_input_block_bits, false,
                  BrotliCompressorImpl::EncoderMode::Default);

  uint64_t input_size = 0;
  std::string original_text{};
  for (uint64_t i = 0; i < 10; i++) {
    TestUtility::feedBufferWithRandomCharacters(buffer, default_input_size * i, i);
    original_text.append(buffer.toString());
    ASSERT_EQ(default_input_size * i, buffer.length());
    input_size += buffer.length();
    compressor.compress(buffer, State::Flush);
    accumulation_buffer.add(buffer);
    drainBuffer(buffer);
    ASSERT_EQ(0, buffer.length());
  }

  compressor.compress(buffer, State::Finish);
  accumulation_buffer.add(buffer);
  drainBuffer(buffer);

  Envoy::Decompressor::BrotliDecompressorImpl decompressor;
  decompressor.init(false);

  decompressor.decompress(accumulation_buffer, buffer);
  std::string decompressed_text{buffer.toString()};

  ASSERT_EQ(original_text.length(), decompressed_text.length());
  EXPECT_EQ(original_text, decompressed_text);
}

} // namespace
} // namespace Compressor
} // namespace Envoy