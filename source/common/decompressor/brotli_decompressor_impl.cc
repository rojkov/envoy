#include "common/decompressor/brotli_decompressor_impl.h"

#include <memory>

#include "envoy/common/exception.h"

#include "common/common/assert.h"
#include "common/common/stack_array.h"

namespace Envoy {
namespace Decompressor {

BrotliDecompressorImpl::BrotliDecompressorImpl() : BrotliDecompressorImpl(4096) {}

BrotliDecompressorImpl::BrotliDecompressorImpl(uint64_t chunk_size)
    : chunk_size_{chunk_size}, initialized_{false},
      state_(BrotliDecoderCreateInstance(NULL, NULL, NULL), &BrotliDecoderDestroyInstance) {
}

void BrotliDecompressorImpl::init() {
  BROTLI_BOOL result(BROTLI_FALSE);

  ASSERT(initialized_ == false);
  result = BrotliDecoderSetParameter(state_.get(), BROTLI_DECODER_PARAM_DISABLE_RING_BUFFER_REALLOCATION, BROTLI_FALSE);
  RELEASE_ASSERT(result == BROTLI_TRUE, "");
  initialized_ = true;
}

void BrotliDecompressorImpl::decompress(const Buffer::Instance& input_buffer,
                                      Buffer::Instance& output_buffer) {
  BrotliContext ctx(chunk_size_);
  const uint64_t num_slices = input_buffer.getRawSlices(nullptr, 0);
  STACK_ARRAY(slices, Buffer::RawSlice, num_slices);
  input_buffer.getRawSlices(slices.begin(), num_slices);

  for (const Buffer::RawSlice& input_slice : slices) {
    ctx.avail_in = input_slice.len_;
    ctx.next_in = static_cast<uint8_t*>(input_slice.mem_);

    while (ctx.avail_in > 0) {
      process(ctx, output_buffer);
    }
  }

  do {
    process(ctx, output_buffer);
  } while (BrotliDecoderHasMoreOutput(state_.get()) && !BrotliDecoderIsFinished(state_.get()));

  const size_t n_output = chunk_size_ - ctx.avail_out;
  if (n_output > 0) {
    output_buffer.add(static_cast<void*>(ctx.chunk_ptr.get()), n_output);
  }
  ENVOY_LOG(debug, "length: {}, out: {}", output_buffer.length(), "redacted");
}

void BrotliDecompressorImpl::process(BrotliContext& ctx, Buffer::Instance& output_buffer) {
  BrotliDecoderResult result;
  result = BrotliDecoderDecompressStream(state_.get(), &ctx.avail_in, &ctx.next_in, &ctx.avail_out,
                                     &ctx.next_out, nullptr);
  switch (result) {
  case BROTLI_DECODER_RESULT_ERROR:
    ENVOY_LOG(error, "error");
    break;
  case BROTLI_DECODER_RESULT_SUCCESS:
    ENVOY_LOG(error, "success");
    break;
  case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
    ENVOY_LOG(error, "needs more input");
    break;
  case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
    ENVOY_LOG(error, "needs more output");
    break;
  default:
    ENVOY_LOG(error, "unknown");
  }
  if (ctx.avail_out == 0) {
    // update output and reset context
    output_buffer.add(static_cast<void*>(ctx.chunk_ptr.get()), chunk_size_);
    ctx.chunk_ptr = std::make_unique<uint8_t[]>(chunk_size_);
    ctx.avail_out = chunk_size_;
    ctx.next_out = ctx.chunk_ptr.get();
  }
}

} // namespace Decompressor
} // namespace Envoy
