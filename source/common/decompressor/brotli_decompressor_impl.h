#pragma once

#include "envoy/decompressor/decompressor.h"

#include "brotli/decode.h"

namespace Envoy {
namespace Decompressor {

struct BrotliContext {
  BrotliContext(size_t chunk_size)
      : chunk_ptr(new uint8_t[chunk_size]), next_in(nullptr), next_out(nullptr), avail_in(0),
        avail_out(chunk_size) {
    next_out = chunk_ptr.get();
  }

  std::unique_ptr<uint8_t[]> chunk_ptr;
  const uint8_t* next_in;
  uint8_t* next_out;
  size_t avail_in;
  size_t avail_out;
};

/**
 * Implementation of decompressor's interface.
 */
class BrotliDecompressorImpl : public Decompressor {
public:
  BrotliDecompressorImpl();

  /**
   * Constructor that allows setting the size of decompressor's output buffer. It
   * should be called whenever a buffer size different than the 4096 bytes, normally set by the
   * default constructor, is desired. If memory is available and it makes sense to output large
   * chunks of compressed data.
   * @param chunk_size amount of memory reserved for the decompressor output.
   */
  BrotliDecompressorImpl(uint64_t chunk_size);

  /**
   * Init must be called in order to initialize the decompressor. Once decompressor is initialized,
   * it cannot be initialized again. Init should run before decompressing any data.
   * @param disable_ring_buffer_reallocation if true disables "canny" ring buffer allocation
   * strategy. Ring buffer is allocated according to window size, despite the real size of the
   * content.
   */
  void init(bool disable_ring_buffer_reallocation);

  // Decompressor
  void decompress(const Buffer::Instance& input_buffer, Buffer::Instance& output_buffer) override;

private:
  void process(BrotliContext& ctx, Buffer::Instance& output_buffer);

  const uint64_t chunk_size_;
  bool initialized_;
  std::unique_ptr<BrotliDecoderState, decltype(&BrotliDecoderDestroyInstance)> state_;
};

} // namespace Decompressor
} // namespace Envoy
