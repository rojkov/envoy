#include "openssl/ssl.h"

#ifdef OPENSSL_IS_BORINGSSL
#include "extensions/filters/listener/tls_inspector/boringssl/tls_inspector.h"
#else
#include "extensions/filters/listener/tls_inspector/openssl/tls_inspector.h"
#endif
