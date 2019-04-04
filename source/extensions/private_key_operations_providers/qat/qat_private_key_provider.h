#pragma once

#include "envoy/api/api.h"
#include "envoy/event/dispatcher.h"
#include "envoy/ssl/private_key/private_key.h"
#include "envoy/ssl/private_key/private_key_config.h"

#include "source/extensions/private_key_operations_providers/qat/qat.pb.h"

#include "extensions/private_key_operations_providers/qat/qat.h"

namespace Envoy {
namespace Extensions {
namespace PrivateKeyOperationsProviders {

class QatPrivateKeyOperations : public virtual Ssl::PrivateKeyOperations {
public:
  QatPrivateKeyOperations(Ssl::PrivateKeyOperationsCallbacks& cb, Event::Dispatcher& dispatcher,
                          QatHandle& handle);

  // Ssl::PrivateKeyOperations
  Ssl::PrivateKeyMethodSharedPtr getPrivateKeyMethods(SSL* ssl) override;

  void registerCallback(QatContext* ctx);
  void unregisterCallback();
  QatHandle& getHandle() { return handle_; };

private:
  Ssl::PrivateKeyMethodSharedPtr ops_{};
  Ssl::PrivateKeyOperationsCallbacks& cb_;

  Event::Dispatcher& dispatcher_;
  Event::FileEventPtr ssl_async_event_{};
  QatHandle& handle_;
};

class QatPrivateKeyOperationsProvider : public virtual Ssl::PrivateKeyOperationsProvider {
public:
  QatPrivateKeyOperationsProvider(
      const qat::QatPrivateKeyOperationsConfig& config,
      Server::Configuration::TransportSocketFactoryContext& private_key_provider_context);
  // Ssl::PrivateKeyOperationsProvider
  Ssl::PrivateKeyOperationsPtr getPrivateKeyOperations(Ssl::PrivateKeyOperationsCallbacks& cb,
                                                       Event::Dispatcher& dispatcher) override;

private:
  std::shared_ptr<QatManager> manager_;
  std::shared_ptr<QatSection> section_;
  std::string section_name_;
  uint32_t poll_delay_;
  Api::Api& api_;
};

} // namespace PrivateKeyOperationsProviders
} // namespace Extensions
} // namespace Envoy
