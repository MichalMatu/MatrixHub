#pragma once

#include <NetworkClientSecure.h>

namespace TELEGRAM {

class TelegramTlsConfig {
public:
    static void configure(NetworkClientSecure& client);
    static bool connectResolved(NetworkClientSecure& client, IPAddress ip, uint16_t port, const char* host);

#if defined(TELEGRAM_TLS_VERIFY) && TELEGRAM_TLS_VERIFY
    static void configureTlsWithRootCa(NetworkClientSecure& client);
#endif
};

}  // namespace TELEGRAM
