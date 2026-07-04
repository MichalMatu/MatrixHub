#pragma once
#include <WiFiClientSecure.h>

namespace NOTIFICATIONS {

class PushoverTlsConfig {
public:
    /**
     * Configure TLS for Pushover connection.
     * Sets the Root CA certificate for secure verification.
     */
    static void configure(WiFiClientSecure& client);

    /**
     * Get Root CA certificate PEM for Pushover API.
     */
    static const char* getRootCaPem();
};

} // namespace NOTIFICATIONS
