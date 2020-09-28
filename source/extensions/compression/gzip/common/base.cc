#include "extensions/compression/gzip/common/base.h"

namespace Envoy {
namespace Zlib {

Base::Base(uint64_t chunk_size, std::function<void(z_stream*)> zstream_deleter)
    : chunk_size_{chunk_size}, zstream_ptr_(new z_stream(), zstream_deleter) {}

uint64_t Base::checksum() { return zstream_ptr_->adler; }

void Base::updateOutput(Buffer::Instance& output_buffer) {
  const uint64_t n_output = chunk_size_ - zstream_ptr_->avail_out;
  if (n_output == 0) {
    return;
  }

  slice_.len_ = n_output;
  output_buffer.commit(&slice_, 1);
  zstream_ptr_->avail_out = chunk_size_;
  uint64_t reserved_slices_num = output_buffer.reserve(chunk_size_, &slice_, 1);
  ASSERT(reserved_slices_num == 1);
  ASSERT(slice_.len_ >= chunk_size_);
  zstream_ptr_->next_out = static_cast<Bytef*>(slice_.mem_);
}

} // namespace Zlib
} // namespace Envoy
