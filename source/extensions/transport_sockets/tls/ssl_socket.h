#include "openssl/ssl.h"

#ifdef OPENSSL_IS_BORINGSSL
#include "extensions/transport_sockets/tls/boringssl/ssl_socket.h"
#else
#include "extensions/transport_sockets/tls/openssl/ssl_socket.h"
#endif
