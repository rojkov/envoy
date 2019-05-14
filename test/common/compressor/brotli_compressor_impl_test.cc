#include "common/buffer/buffer_impl.h"
#include "common/common/stack_array.h"
#include "common/compressor/brotli_compressor_impl.h"

#include "test/test_common/utility.h"

#include "gtest/gtest.h"

namespace Envoy {
namespace Compressor {
namespace {

class BrotliCompressorImplTest : public testing::Test {};

TEST_F(BrotliCompressorImplTest, CallingFinishOnly) {
  Buffer::OwnedImpl buffer;

  BrotliCompressorImpl compressor;
  compressor.init();
  TestUtility::feedBufferWithRandomCharacters(buffer, 15096);
  compressor.compress(buffer, State::Finish);
  buffer.drain(buffer.length());
}

TEST_F(BrotliCompressorImplTest, CallingFlushOnly) {
  Buffer::OwnedImpl buffer;

  BrotliCompressorImpl compressor;
  compressor.init();
  TestUtility::feedBufferWithRandomCharacters(buffer, 15096);
  compressor.compress(buffer, State::Flush);
  buffer.drain(buffer.length());
}

} // namespace
} // namespace Compressor
} // namespace Envoy