#include "common/buffer/buffer_impl.h"
#include "common/common/stack_array.h"
#include "common/compressor/qatzip_compressor_impl.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Compressor {
namespace {

class QatzipCompressorImplTest : public testing::Test {
protected:
  void drainBuffer(Buffer::OwnedImpl& buffer) { buffer.drain(buffer.length()); }

  static const uint32_t default_quality{11};
  static const uint32_t default_window_bits{22};
  static const uint32_t default_input_block_bits{22};
  static const uint64_t default_input_size{796};
};

TEST_F(QatzipCompressorImplTest, CallingFinishOnly) {
  Buffer::OwnedImpl buffer;
  QatzipCompressorImpl compressor;

  compressor.init(1, 64 * 1024, 128 * 1024, 128);
  TestUtility::feedBufferWithRandomCharacters(buffer, 4096);
  compressor.compress(buffer, State::Finish);
}

TEST_F(QatzipCompressorImplTest, CallingFlushOnly) {
  Buffer::OwnedImpl buffer;
  QatzipCompressorImpl compressor;

  compressor.init(1, 64 * 1024, 128 * 1024, 128);
  TestUtility::feedBufferWithRandomCharacters(buffer, 4096);
  compressor.compress(buffer, State::Flush);
}

TEST_F(QatzipCompressorImplTest, CompressWithSmallChunkSize) {
  Buffer::OwnedImpl buffer;
  Buffer::OwnedImpl accumulation_buffer;
  QatzipCompressorImpl compressor(64);

  compressor.init(1, 64 * 1024, 128 * 1024, 128);

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
}

} // namespace
} // namespace Compressor
} // namespace Envoy