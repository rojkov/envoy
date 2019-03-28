#include "openssl/ssl.h"

#ifdef OPENSSL_IS_BORINGSSL
#include "extensions/transport_sockets/tls/boringssl/config.h"
#else
#include "extensions/transport_sockets/tls/openssl/config.h"
#endif
