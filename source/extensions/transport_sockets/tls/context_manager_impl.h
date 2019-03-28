#include "openssl/ssl.h"

#ifdef OPENSSL_IS_BORINGSSL
#include "extensions/transport_sockets/tls/boringssl/context_manager_impl.h"
#else
#include "extensions/transport_sockets/tls/openssl/context_manager_impl.h"
#endif
