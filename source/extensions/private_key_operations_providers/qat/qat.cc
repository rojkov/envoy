#include "extensions/private_key_operations_providers/qat/qat.h"

#include "rsa/internal.h"

namespace Envoy {
namespace Extensions {
namespace PrivateKeyOperationsProviders {

int QatManager::ssl_qat_provider_index = -1;
int QatManager::ssl_qat_context_index = -1;

void QatManager::qatPoll(QatHandle& handle, uint32_t poll_delay) {

  if (poll_delay == 0) {
    // A reasonable default value.
    poll_delay = 2000000;
  }

  while (1) {
    {
      Thread::LockGuard poll_lock(handle.poll_lock_);

      if (handle.done_) {
        return;
      }

      // Wait for an event which tells that a QAT request has been made.
      if (handle.users_ == 0) {
        handle.qat_thread_cond_.wait(handle.poll_lock_);
      }
    }

    icp_sal_CyPollInstance(handle.handle_, 0);

    // Sleep for a while.
    std::this_thread::sleep_for(std::chrono::nanoseconds(poll_delay)); // NO_CHECK_FORMAT(real_time)
  }
}

QatHandle::~QatHandle() {
  if (polling_thread_ == nullptr) {
    return;
  }
  {
    Thread::LockGuard poll_lock(poll_lock_);
    done_ = true;
    qat_thread_cond_.notifyOne();
  }
  polling_thread_->join();

  cpaCyStopInstance(handle_);
};

QatSection::QatSection(std::string name) : name_(name), qat_handles_(nullptr), nextHandle_(0) {}

QatSection::~QatSection() { delete[] qat_handles_; };

bool QatSection::startSection(Api::Api& api, uint32_t poll_delay) {
  // This function is called from a single-thread environment (server startup) to initialize QAT for
  // this particular section.

  CpaStatus status = icp_sal_userStart(name_.c_str());
  if (status != CPA_STATUS_SUCCESS) {
    return false;
  }

  status = cpaCyGetNumInstances(&num_instances_);
  if ((status != CPA_STATUS_SUCCESS) || (num_instances_ <= 0)) {
    return false;
  }

  qat_handles_ = new QatHandle[num_instances_];

  CpaInstanceHandle* handles = new CpaInstanceHandle[num_instances_];

  status = cpaCyGetInstances(num_instances_, handles);
  if (status != CPA_STATUS_SUCCESS) {
    delete[] handles;
    return false;
  }

  for (int i = 0; i < num_instances_; i++) {
    status = cpaCySetAddressTranslation(handles[i], qaeVirtToPhysNUMA);
    if (status != CPA_STATUS_SUCCESS) {
      delete[] handles;
      return false;
    }

    qat_handles_[i].handle_ = handles[i];

    status = cpaCyInstanceGetInfo2(handles[i], &(qat_handles_[i].info_));
    if (status != CPA_STATUS_SUCCESS) {
      delete[] handles;
      return false;
    }

    // TODO(ipuustin): Maybe start only when given to a thread? Should we start them all?
    status = cpaCyStartInstance(handles[i]);
    if (status != CPA_STATUS_SUCCESS) {
      delete[] handles;
      return false;
    }

    qat_handles_[i].polling_thread_ =
        api.threadFactory().createThread([this, poll_delay, i]() -> void {
          QatManager::qatPoll(this->qat_handles_[i], poll_delay);
        });
  }

  delete[] handles;

  return true;
}

bool QatSection::isInitialized() { return num_instances_ > 0; }

void QatSection::forEachHandle(std::function<void(QatHandle*)> f) const {
  for (size_t i = 0; i < num_instances_; i++) {
    f(&qat_handles_[i]);
  }
}

QatHandle& QatSection::getNextHandle() {
  Thread::LockGuard handle_lock(handle_lock_);
  if (nextHandle_ == num_instances_) {
    nextHandle_ = 0;
  }
  return qat_handles_[nextHandle_++];
}

QatManager::QatManager() {
  // Initialize the indexes for the data we need to keep within the SSL context.
  QatManager::ssl_qat_provider_index = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
  QatManager::ssl_qat_context_index = SSL_get_ex_new_index(0, nullptr, nullptr, nullptr, nullptr);
}

QatManager::~QatManager() {
  // The idea is that icp_sal_userStop() is called after the instances have been stopped and the
  // polling threads exited.
  section_map_.clear();

  icp_sal_userStop();
}

std::shared_ptr<QatSection> QatManager::findSection(std::string name) {
  auto it = section_map_.find(name);
  if (it == section_map_.end()) {
    return nullptr;
  }
  return it->second;
}

std::shared_ptr<QatSection> QatManager::addSection(std::string name) {
  std::shared_ptr<QatSection> section = std::make_shared<QatSection>(name);

  section_map_.insert(std::make_pair(name, section)).first;
  return section;
}

// The decrypt operation buffer creation functions are partially adapted from
// OpenSSL QAT engine. Changes include using the QAT library functions for
// allocating memory.

bool QatContext::convertBnToFlatbuffer(CpaFlatBuffer* fb, const BIGNUM* bn) {
  fb->dataLenInBytes = static_cast<Cpa32U>(BN_num_bytes(bn));
  if (fb->dataLenInBytes == 0) {
    fb->pData = nullptr;
    return false;
  }
  fb->pData =
      static_cast<Cpa8U*>(qaeMemAllocNUMA(fb->dataLenInBytes, handle_.info_.nodeAffinity, 64));
  if (fb->pData == nullptr) {
    fb->dataLenInBytes = 0;
    return false;
  }

  if (BN_bn2bin(bn, fb->pData) == 0) {
    fb->dataLenInBytes = 0;
    return false;
  }

  return true;
}

static void freeNuma(void* ptr) {
  if (ptr) {
    qaeMemFreeNUMA(&ptr);
  }
}

void QatContext::freeDecryptOpBuf(CpaCyRsaDecryptOpData* dec_op_data, CpaFlatBuffer* out_buf) {
  CpaCyRsaPrivateKeyRep2* key = nullptr;

  if (dec_op_data) {
    if (dec_op_data->inputData.pData)
      freeNuma(dec_op_data->inputData.pData);

    if (dec_op_data->pRecipientPrivateKey) {
      key = &dec_op_data->pRecipientPrivateKey->privateKeyRep2;
      freeNuma(static_cast<void*>(key->prime1P.pData));
      freeNuma(static_cast<void*>(key->prime2Q.pData));
      freeNuma(static_cast<void*>(key->exponent1Dp.pData));
      freeNuma(static_cast<void*>(key->exponent2Dq.pData));
      freeNuma(static_cast<void*>(key->coefficientQInv.pData));
      OPENSSL_free(dec_op_data->pRecipientPrivateKey);
    }
    OPENSSL_free(dec_op_data);
  }

  if (out_buf) {
    if (out_buf->pData)
      freeNuma(out_buf->pData);
    OPENSSL_free(out_buf);
  }
}

bool QatContext::buildDecryptOpBuf(int flen, const unsigned char* from, RSA* rsa, int padding,
                                   CpaCyRsaDecryptOpData** dec_op_data,
                                   CpaFlatBuffer** output_buffer, int alloc_pad) {
  int rsa_len;
  int padding_result;
  CpaCyRsaPrivateKey* cpa_prv_key;
  const BIGNUM* p = nullptr;
  const BIGNUM* q = nullptr;
  const BIGNUM* dmp1 = nullptr;
  const BIGNUM* dmq1 = nullptr;
  const BIGNUM* iqmp = nullptr;

  RSA_get0_factors(rsa, &p, &q);
  RSA_get0_crt_params(rsa, &dmp1, &dmq1, &iqmp);

  if (p == nullptr || q == nullptr || dmp1 == nullptr || dmq1 == nullptr || iqmp == nullptr) {
    return false;
  }

  *dec_op_data = static_cast<CpaCyRsaDecryptOpData*>(OPENSSL_malloc(sizeof(CpaCyRsaDecryptOpData)));
  if (*dec_op_data == nullptr) {
    return false;
  }
  memset(*dec_op_data, 0, sizeof(**dec_op_data));

  cpa_prv_key = static_cast<CpaCyRsaPrivateKey*>(OPENSSL_malloc(sizeof(CpaCyRsaPrivateKey)));
  if (cpa_prv_key == nullptr) {
    goto error;
  }
  memset(cpa_prv_key, 0, sizeof(*cpa_prv_key));

  (*dec_op_data)->pRecipientPrivateKey = cpa_prv_key;

  cpa_prv_key->version = CPA_CY_RSA_VERSION_TWO_PRIME;
  cpa_prv_key->privateKeyRepType = CPA_CY_RSA_PRIVATE_KEY_REP_TYPE_2;
  if (!convertBnToFlatbuffer(&cpa_prv_key->privateKeyRep2.prime1P, p) ||
      !convertBnToFlatbuffer(&cpa_prv_key->privateKeyRep2.prime2Q, q) ||
      !convertBnToFlatbuffer(&cpa_prv_key->privateKeyRep2.exponent1Dp, dmp1) ||
      !convertBnToFlatbuffer(&cpa_prv_key->privateKeyRep2.exponent2Dq, dmq1) ||
      !convertBnToFlatbuffer(&cpa_prv_key->privateKeyRep2.coefficientQInv, iqmp)) {
    goto error;
  }

  rsa_len = RSA_size(rsa);

  // Works for 64-bit machines.
  (*dec_op_data)->inputData.pData = static_cast<Cpa8U*>(qaeMemAllocNUMA(
      ((padding != RSA_NO_PADDING) && alloc_pad) ? rsa_len : flen, handle_.info_.nodeAffinity, 64));

  if ((*dec_op_data)->inputData.pData == nullptr) {
    goto error;
  }

  (*dec_op_data)->inputData.dataLenInBytes =
      (padding != RSA_NO_PADDING) && alloc_pad ? rsa_len : flen;

  if (alloc_pad) {
    if (padding == RSA_PKCS1_PADDING) {
      padding_result =
          RSA_padding_add_PKCS1_type_1((*dec_op_data)->inputData.pData, rsa_len, from, flen);

    } else if (padding == RSA_NO_PADDING) {
      padding_result = RSA_padding_add_none((*dec_op_data)->inputData.pData, rsa_len, from, flen);
    } else {
      goto error;
    }
  } else {
    padding_result = RSA_padding_add_none((*dec_op_data)->inputData.pData, rsa_len, from, flen);
  }

  if (padding_result <= 0) {
    goto error;
  }

  *output_buffer = static_cast<CpaFlatBuffer*>(OPENSSL_malloc(sizeof(CpaFlatBuffer)));
  if (*output_buffer == nullptr) {
    goto error;
  }
  memset(*output_buffer, 0, sizeof(output_buffer));

  (*output_buffer)->pData =
      static_cast<Cpa8U*>(qaeMemAllocNUMA(rsa_len, handle_.info_.nodeAffinity, 64));
  if ((*output_buffer)->pData == nullptr) {
    goto error;
  }
  (*output_buffer)->dataLenInBytes = rsa_len;

  return true;

error:
  freeDecryptOpBuf(*dec_op_data, *output_buffer);
  return false;
}

static void decrypt_cb(void* pCallbackTag, CpaStatus status, void* pOpData,
                       CpaFlatBuffer* out_buf) {
  // TODO(ipuustin): this whole function is called from the polling thread context. Need to lock
  // access to ctx fully?
  QatContext* ctx = static_cast<QatContext*>(pCallbackTag);
  CpaCyRsaDecryptOpData* op_data = static_cast<CpaCyRsaDecryptOpData*>(pOpData);

  QatHandle& handle = ctx->getHandle();
  {
    Thread::LockGuard poll_lock(handle.poll_lock_);
    handle.users_--;
  }
  {
    Thread::LockGuard data_lock(ctx->data_lock_);
    if (!ctx->copyDecryptedData(out_buf->pData, out_buf->dataLenInBytes)) {
      status = CPA_STATUS_FAIL;
    }

    ctx->freeDecryptOpBuf(op_data, out_buf);

    // Take the fd from the ctx and send the status to it. This indicates that the
    // decryption has completed and the upper layer can redo the SSL request.

    // TODO(ipuustin): OS system calls.
    int ret = write(ctx->getWriteFd(), &status, sizeof(status));
    (void)ret;
  }
}

QatContext::QatContext(QatHandle& handle)
    : handle_(handle), last_status_(CPA_STATUS_RETRY), read_fd_(-1), write_fd_(-1) {}

QatContext::~QatContext() {
  if (read_fd_ >= 0) {
    close(read_fd_);
  }
  if (write_fd_ >= 0) {
    close(write_fd_);
  }
}

bool QatContext::init() {
  // TODO(ipuustin): OS system calls.
  int pipe_fds[2] = {0, 0};
  int ret = pipe(pipe_fds);

  if (ret == -1) {
    return false;
  }

  read_fd_ = pipe_fds[0];
  write_fd_ = pipe_fds[1];

  return true;
}

bool QatContext::decrypt(int len, const unsigned char* from, RSA* rsa, int padding) {
  int ret;
  CpaStatus status;
  CpaCyRsaDecryptOpData* op_data = nullptr;
  CpaFlatBuffer* out_buf = nullptr;

  // TODO(ipuustin): shuld this rather be a class function?
  ret = buildDecryptOpBuf(len, from, rsa, padding, &op_data, &out_buf, 1);
  if (!ret) {
    return false;
  }

  do {
    status = cpaCyRsaDecrypt(handle_.handle_, decrypt_cb, this, op_data, out_buf);
  } while (status == CPA_STATUS_RETRY);

  if (status != CPA_STATUS_SUCCESS) {
    return false;
  }

  {
    Thread::LockGuard poll_lock(handle_.poll_lock_);
    handle_.users_++;
    // Start polling for the result.
    handle_.qat_thread_cond_.notifyOne();
  }

  return true;
}

QatHandle& QatContext::getHandle() { return handle_; };

int QatContext::getDecryptedDataLength() { return decrypted_data_length_; }

unsigned char* QatContext::getDecryptedData() { return decrypted_data_; }

void QatContext::setOpStatus(CpaStatus status) { last_status_ = status; };

CpaStatus QatContext::getOpStatus() { return last_status_; }

bool QatContext::copyDecryptedData(unsigned char* bytes, int len) {
  if (len > QAT_BUFFER_SIZE) {
    return false;
  }
  memcpy(decrypted_data_, bytes, len);
  decrypted_data_length_ = len;
  return true;
};

int QatContext::getFd() { return read_fd_; }

int QatContext::getWriteFd() { return write_fd_; };

} // namespace PrivateKeyOperationsProviders
} // namespace Extensions
} // namespace Envoy
