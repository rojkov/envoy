#pragma once

#include "envoy/compressor/compressor.h"

#include "brotli/encode.h"

namespace Envoy {
namespace Compressor {

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
 * Implementation of compressor's interface.
 */
class BrotliCompressorImpl : public Compressor {
public:
  BrotliCompressorImpl();

  /**
   * Constructor that allows setting the size of compressor's output buffer. It
   * should be called whenever a buffer size different than the 4096 bytes, normally set by the
   * default constructor, is desired.
   * @param chunk_size amount of memory reserved for the compressor output.
   */
  BrotliCompressorImpl(size_t chunk_size);

  /**
   * Enum values are used for setting the encoder mode.
   * generic: in this mode compressor does not know anything in advance about the properties of the
   * input text: compression mode for UTF-8 formatted text input font: compression mode used in
   * WOFF 2.0 default: compression mode used by Broli encoder by default. @see BROTLI_DEFAULT_MODE
   * in brotli manual.
   */
  enum class EncoderMode : uint32_t {
    Generic = BROTLI_MODE_GENERIC,
    Text = BROTLI_MODE_TEXT,
    Font = BROTLI_MODE_FONT,
    Default = BROTLI_DEFAULT_MODE,
  };

  /**
   * Init must be called in order to initialize the compressor. Once compressor is initialized, it
   * cannot be initialized again. Init should run before compressing any data.
   * @param quality sets compression level. The higher the quality, the slower the
   * compression. @see BROTLI_PARAM_QUALITY (brotli manual).
   * @param window_bits sets recommended sliding LZ77 window size.
   * @param input_block_bits sets recommended input block size. Bigger input block size allows
   * better compression, but consumes more memory.
   * @param disable_literal_context_modeling affects usage of "literal context modeling" format
   * feature. This flag is a "decoding-speed vs compression ratio" trade-off.
   * @param mode tunes encoder for specific input. @see EncoderMode enum.
   */
  void init(const uint32_t quality, const uint32_t window_bits, const uint32_t input_block_bits,
            const bool disable_literal_context_modeling, const EncoderMode mode);

  // Compressor
  void compress(Buffer::Instance& buffer, State state) override;

private:
  void process(BrotliContext& ctx, Buffer::Instance& output_buffer,
               const BrotliEncoderOperation op);

  bool initialized_;
  const size_t chunk_size_;
  std::unique_ptr<BrotliEncoderState, decltype(&BrotliEncoderDestroyInstance)> state_;
};

} // namespace Compressor
} // namespace Envoy
