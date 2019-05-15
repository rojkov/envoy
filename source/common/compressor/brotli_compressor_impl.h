#pragma once

#include "envoy/compressor/compressor.h"

#include "common/common/logger.h"

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
class BrotliCompressorImpl : public Compressor, protected Logger::Loggable<Logger::Id::connection> {
public:
  BrotliCompressorImpl();

  enum class EncoderMode : uint32_t {
    Generic = BROTLI_MODE_GENERIC,
    Text = BROTLI_MODE_TEXT,
    Font = BROTLI_MODE_FONT,
    Default = BROTLI_DEFAULT_MODE,
  };

  /**
   * Init must be called in order to initialize the compressor. Once compressor is initialized, it
   * cannot be initialized again. Init should run before compressing any data.
   */
  void init(const uint32_t quality, const uint32_t windowBits, const uint32_t inputBlockBits, const bool disableLiteralContextModeling, const EncoderMode mode);

  // Compressor
  void compress(Buffer::Instance& buffer, State state) override;

private:
  void process(BrotliContext& ctx, Buffer::Instance& output_buffer, const BrotliEncoderOperation op);

  bool initialized_;
  const size_t chunk_size_;
  std::unique_ptr<BrotliEncoderState, decltype(&BrotliEncoderDestroyInstance)> state_;
};

} // namespace Compressor
} // namespace Envoy
