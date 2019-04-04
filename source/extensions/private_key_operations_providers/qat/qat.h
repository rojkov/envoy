#pragma once

#include <map>

#include "envoy/api/api.h"
#include "envoy/singleton/manager.h"

#include "common/common/lock_guard.h"
#include "common/common/thread.h"

#include "extensions/transport_sockets/tls/utility.h"

// #include "common/common/logger.h"
#include "openssl/err.h"
#include "openssl/rand.h"
#include "openssl/x509v3.h"

#include "quickassist/include/cpa.h"
#include "quickassist/include/lac/cpa_cy_rsa.h"
#include "quickassist/lookaside/access_layer/include/icp_sal_user.h"
#include "quickassist/lookaside/access_layer/include/icp_sal_poll.h"
#include "quickassist/include/lac/cpa_cy_im.h"
#include "quickassist/utilities/libusdm_drv/qae_mem.h"

namespace Envoy {
namespace Extensions {
namespace PrivateKeyOperationsProviders {

const int QAT_BUFFER_SIZE = 1024;

/**
 * Represents a QAT hardware instance.
 */
class QatHandle {
public:
  ~QatHandle();
  // TODO(ipuustin): getters and setters
  CpaInstanceHandle handle_;
  CpaInstanceInfo2 info_;
  Thread::ThreadPtr polling_thread_{};

  Thread::MutexBasicLockable poll_lock_{};
  Thread::CondVar qat_thread_cond_{};

  bool done_{};
  int users_{};
};

class QatSection {
public:
  QatSection(std::string name);
  ~QatSection();
  bool startSection(Api::Api& api, uint32_t poll_delay);
  void forEachHandle(std::function<void(QatHandle*)> f) const;
  QatHandle& getNextHandle();
  bool isInitialized();

private:
  std::string name_{};
  Thread::MutexBasicLockable handle_lock_{};
  Cpa16U num_instances_{};
  QatHandle* qat_handles_{};
  int nextHandle_{};
};

/**
 * Manages the global QAT operations.
 */
class QatManager : public std::enable_shared_from_this<QatManager>, public Singleton::Instance {
public:
  static void qatPoll(QatHandle& handle, uint32_t poll_delay);
  static int ssl_qat_provider_index;
  static int ssl_qat_context_index;

  QatManager();
  ~QatManager();
  std::shared_ptr<QatSection> findSection(std::string name);
  std::shared_ptr<QatSection> addSection(std::string name);

private:
  std::map<std::string, std::shared_ptr<QatSection>> section_map_{};
};

/**
 * Represents a single QAT operation context.
 */
class QatContext {
public:
  QatContext(QatHandle& handle);
  ~QatContext();
  bool init();
  QatHandle& getHandle();
  int getDecryptedDataLength();
  unsigned char* getDecryptedData();
  bool copyDecryptedData(unsigned char* bytes, int len);
  void setOpStatus(CpaStatus status);
  CpaStatus getOpStatus();
  int getFd();
  int getWriteFd();
  bool decrypt(int len, const unsigned char* from, RSA* rsa, int padding);
  void freeDecryptOpBuf(CpaCyRsaDecryptOpData* dec_op_data, CpaFlatBuffer* out_buf);

  Thread::MutexBasicLockable data_lock_{};

private:
  // TODO(ipuustin): QatHandle might be a more logical place for these.
  bool convertBnToFlatbuffer(CpaFlatBuffer* fb, const BIGNUM* bn);
  bool buildDecryptOpBuf(int flen, const unsigned char* from, RSA* rsa, int padding,
                         CpaCyRsaDecryptOpData** dec_op_data, CpaFlatBuffer** output_buffer,
                         int alloc_pad);

  QatHandle& handle_;
  CpaStatus last_status_;
  unsigned char decrypted_data_[QAT_BUFFER_SIZE];
  int decrypted_data_length_;
  // Pipe for passing the message that the operation is completed.
  int read_fd_;
  int write_fd_;
};

} // namespace PrivateKeyOperationsProviders
} // namespace Extensions
} // namespace Envoy
