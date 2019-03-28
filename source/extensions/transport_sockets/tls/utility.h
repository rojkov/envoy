#include "openssl/ssl.h"

#ifdef OPENSSL_IS_BORINGSSL
#include "extensions/transport_sockets/tls/boringssl/utility.h"
#else
#include "extensions/transport_sockets/tls/openssl/utility.h"
#endif
