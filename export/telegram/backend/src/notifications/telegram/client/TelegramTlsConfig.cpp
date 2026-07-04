#include "TelegramTlsConfig.h"
#include "../../../config/App.h"
#include "../../../system/logging/Logging.h"

namespace TELEGRAM {



#if TELEGRAM_TLS_VERIFY
// NOTE: We intentionally avoid setCACertBundle() here.
// On this partition layout (2MB app), enabling CA bundle support pulls in
// significant additional code and can make the firmware non-bootable.
// A pinned Root CA PEM is much smaller and still provides real certificate validation.
static const char kTelegramRootCaPem[] PROGMEM =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIEfTCCA2WgAwIBAgIDG+cVMA0GCSqGSIb3DQEBCwUAMGMxCzAJBgNVBAYTAlVT\n"
    "MSEwHwYDVQQKExhUaGUgR28gRGFkZHkgR3JvdXAsIEluYy4xMTAvBgNVBAsTKEdv\n"
    "IERhZGR5IENsYXNzIDIgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTQwMTAx\n"
    "MDcwMDAwWhcNMzEwNTMwMDcwMDAwWjCBgzELMAkGA1UEBhMCVVMxEDAOBgNVBAgT\n"
    "B0FyaXpvbmExEzARBgNVBAcTClNjb3R0c2RhbGUxGjAYBgNVBAoTEUdvRGFkZHku\n"
    "Y29tLCBJbmMuMTEwLwYDVQQDEyhHbyBEYWRkeSBSb290IENlcnRpZmljYXRlIEF1\n"
    "dGhvcml0eSAtIEcyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAv3Fi\n"
    "CPH6WTT3G8kYo/eASVjpIoMTpsUgQwE7hPHmhUmfJ+r2hBtOoLTbcJjHMgGxBT4H\n"
    "Tu70+k8vWTAi56sZVmvigAf88xZ1gDlRe+X5NbZ0TqmNghPktj+pA4P6or6KFWp/\n"
    "3gvDthkUBcrqw6gElDtGfDIN8wBmIsiNaW02jBEYt9OyHGC0OPoCjM7T3UYH3go+\n"
    "6118yHz7sCtTpJJiaVElBWEaRIGMLKlDliPfrDqBmg4pxRyp6V0etp6eMAo5zvGI\n"
    "gPtLXcwy7IViQyU0AlYnAZG0O3AqP26x6JyIAX2f1PnbU21gnb8s51iruF9G/M7E\n"
    "GwM8CetJMVxpRrPgRwIDAQABo4IBFzCCARMwDwYDVR0TAQH/BAUwAwEB/zAOBgNV\n"
    "HQ8BAf8EBAMCAQYwHQYDVR0OBBYEFDqahQcQZyi27/a9BUFuIMGU2g/eMB8GA1Ud\n"
    "IwQYMBaAFNLEsNKR1EwRcbNhyz2h/t2oatTjMDQGCCsGAQUFBwEBBCgwJjAkBggr\n"
    "BgEFBQcwAYYYaHR0cDovL29jc3AuZ29kYWRkeS5jb20vMDIGA1UdHwQrMCkwJ6Al\n"
    "oCOGIWh0dHA6Ly9jcmwuZ29kYWRkeS5jb20vZ2Ryb290LmNybDBGBgNVHSAEPzA9\n"
    "MDsGBFUdIAAwMzAxBggrBgEFBQcCARYlaHR0cHM6Ly9jZXJ0cy5nb2RhZGR5LmNv\n"
    "bS9yZXBvc2l0b3J5LzANBgkqhkiG9w0BAQsFAAOCAQEAWQtTvZKGEacke+1bMc8d\n"
    "H2xwxbhuvk679r6XUOEwf7ooXGKUwuN+M/f7QnaF25UcjCJYdQkMiGVnOQoWCcWg\n"
    "OJekxSOTP7QYpgEGRJHjp2kntFolfzq3Ms3dhP8qOCkzpN1nsoX+oYggHFCJyNwq\n"
    "9kIDN0zmiN/VryTyscPfzLXs4Jlet0lUIDyUGAzHHFIYSaRt4bNYC8nY7NmuHDKO\n"
    "KHAN4v6mF56ED71XcLNa6R+ghlO773z/aQvgSMO3kwvIClTErF0UZzdsyqUvMQg3\n"
    "qm5vjLyb4lddJIGvl5echK1srDdMZvNhkREg5L4wn3qkKQmw4TRfZHcYQFHfjDCm\n"
    "rw==\n"
    "-----END CERTIFICATE-----\n";

void TelegramTlsConfig::configureTlsWithRootCa(NetworkClientSecure& client) {
    client.setCACert(kTelegramRootCaPem);
}
#endif

void TelegramTlsConfig::configure(NetworkClientSecure& client) {
    static bool logged = false;
    
    client.setHandshakeTimeout(APP::NOTIFICATIONS::TELEGRAM_TLS_HANDSHAKE_TIMEOUT_SEC);
    client.setTimeout(APP::NOTIFICATIONS::TELEGRAM_HTTP_IO_TIMEOUT_MS);

#if TELEGRAM_TLS_VERIFY
    configureTlsWithRootCa(client);
    if (!logged) {
        LOGD("[Telegram] TLS configured (Root CA verify enabled)");
        logged = true;
    }
#else
    client.setInsecure();
    if (!logged) {
        LOGD("[Telegram] TLS configured (insecure mode)");
        logged = true;
    }
#endif
}

bool TelegramTlsConfig::connectResolved(NetworkClientSecure& client, IPAddress ip, uint16_t port, const char* host) {
#if TELEGRAM_TLS_VERIFY
    return client.connect(ip, port, host, kTelegramRootCaPem, nullptr, nullptr);
#else
    return client.connect(ip, port, host, nullptr, nullptr, nullptr);
#endif
}

}  // namespace TELEGRAM
