#include "common/compressor/brotli_compressor_impl.h"

#include <memory>

#include "common/common/assert.h"
#include "common/common/stack_array.h"

namespace Envoy {
namespace Compressor {

BrotliCompressorImpl::BrotliCompressorImpl()
    : initialized_{false}, chunk_size_{4096},
      state_(BrotliEncoderCreateInstance(NULL, NULL, NULL), &BrotliEncoderDestroyInstance) {}

void BrotliCompressorImpl::init(uint32_t quality, uint32_t windowBits, uint32_t inputBlockBits, EncoderMode mode) {
  BROTLI_BOOL result(BROTLI_FALSE);

  ASSERT(initialized_ == false);
  ASSERT(quality <= BROTLI_MAX_QUALITY);
  result = BrotliEncoderSetParameter(state_.get(), BROTLI_PARAM_QUALITY, quality);
  RELEASE_ASSERT(result == BROTLI_TRUE, "");
  ASSERT(windowBits >= BROTLI_MIN_WINDOW_BITS && windowBits <= BROTLI_MAX_WINDOW_BITS);
  result = BrotliEncoderSetParameter(state_.get(), BROTLI_PARAM_LGWIN, windowBits);
  RELEASE_ASSERT(result == BROTLI_TRUE, "");
  ASSERT(inputBlockBits >= BROTLI_MIN_INPUT_BLOCK_BITS && inputBlockBits <= BROTLI_MAX_INPUT_BLOCK_BITS);
  result = BrotliEncoderSetParameter(state_.get(), BROTLI_PARAM_LGBLOCK, inputBlockBits);
  RELEASE_ASSERT(result == BROTLI_TRUE, "");
  result = BrotliEncoderSetParameter(state_.get(), BROTLI_PARAM_MODE, static_cast<uint32_t>(mode));
  RELEASE_ASSERT(result == BROTLI_TRUE, "");

  initialized_ = true;
}

void BrotliCompressorImpl::compress(Buffer::Instance& buffer, State state) {
  BrotliContext ctx(chunk_size_);

  const uint64_t num_slices = buffer.getRawSlices(nullptr, 0);
  STACK_ARRAY(slices, Buffer::RawSlice, num_slices);
  buffer.getRawSlices(slices.begin(), num_slices);

  for (const Buffer::RawSlice& input_slice : slices) {
    ctx.avail_in = input_slice.len_;
    ctx.next_in = static_cast<uint8_t*>(input_slice.mem_);

    while (ctx.avail_in > 0) {
      process(ctx, buffer, BROTLI_OPERATION_PROCESS);
    }

    buffer.drain(input_slice.len_);
  }

  do {
    process(ctx, buffer, state == State::Finish ? BROTLI_OPERATION_FINISH : BROTLI_OPERATION_FLUSH);
  } while (BrotliEncoderHasMoreOutput(state_.get()) && !BrotliEncoderIsFinished(state_.get()));

  const size_t n_output = chunk_size_ - ctx.avail_out;
  if (n_output > 0) {
    buffer.add(static_cast<void*>(ctx.chunk_ptr.get()), n_output);
  }
  ENVOY_LOG(debug, "length: {}, out: {}", buffer.length(), "redacted");
}

void BrotliCompressorImpl::process(BrotliContext& ctx, Buffer::Instance& output_buffer,
                                   const BrotliEncoderOperation op) {
  ASSERT(BrotliEncoderCompressStream(state_.get(), op, &ctx.avail_in, &ctx.next_in, &ctx.avail_out,
                                     &ctx.next_out, nullptr) == BROTLI_TRUE);
  if (ctx.avail_out == 0) {
    // update output and reset context
    output_buffer.add(static_cast<void*>(ctx.chunk_ptr.get()), chunk_size_);
    ctx.chunk_ptr = std::make_unique<uint8_t[]>(chunk_size_);
    ctx.avail_out = chunk_size_;
    ctx.next_out = ctx.chunk_ptr.get();
  }
}

} // namespace Compressor
} // namespace Envoy
