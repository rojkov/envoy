#include "extensions/private_key_operations_providers/qat/config.h"

#include <memory>

#include "envoy/registry/registry.h"

#include "common/config/utility.h"
#include "common/protobuf/utility.h"

#include "source/extensions/private_key_operations_providers/qat/qat.pb.h"
#include "source/extensions/private_key_operations_providers/qat/qat.pb.validate.h"

#include "openssl/ssl.h"

namespace Envoy {
namespace Extensions {
namespace PrivateKeyOperationsProviders {

Ssl::PrivateKeyOperationsProviderSharedPtr
QatPrivateKeyOperationsFactory::createPrivateKeyOperationsProviderInstance(
    const envoy::api::v2::auth::PrivateKeyOperations& message,
    Server::Configuration::TransportSocketFactoryContext& private_key_provider_context) {
  (void)private_key_provider_context;
  ProtobufTypes::MessagePtr proto_config = std::make_unique<qat::QatPrivateKeyOperationsConfig>();

  Config::Utility::translateOpaqueConfig(message.typed_config(), ProtobufWkt::Struct(),
                                         *proto_config);
  const qat::QatPrivateKeyOperationsConfig conf =
      MessageUtil::downcastAndValidate<const qat::QatPrivateKeyOperationsConfig&>(*proto_config);

  return std::make_shared<QatPrivateKeyOperationsProvider>(conf, private_key_provider_context);
}

REGISTER_FACTORY(QatPrivateKeyOperationsFactory, Ssl::PrivateKeyOperationsProviderInstanceFactory);

} // namespace PrivateKeyOperationsProviders
} // namespace Extensions
} // namespace Envoy
