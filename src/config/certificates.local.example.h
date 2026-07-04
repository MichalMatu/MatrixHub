#ifndef CERTIFICATES_LOCAL_H
#define CERTIFICATES_LOCAL_H

// Copy this file to src/config/certificates.local.h and replace both PEM values
// with a device-specific self-signed certificate and private key before building.
// Do not commit certificates.local.h.

const char* server_cert = "REPLACE_WITH_DEVICE_CERTIFICATE_PEM";
const char* server_key = "REPLACE_WITH_DEVICE_PRIVATE_KEY_PEM";

#endif
