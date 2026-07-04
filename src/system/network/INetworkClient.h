#pragma once

/**
 * @file INetworkClient.h
 * @brief Abstract interface for HTTP network operations
 * 
 * Provides a unified interface for HTTP clients to enable:
 * - Dependency injection in services
 * - Testability (mock implementations)
 * - Consistent API across the codebase
 */

#include <stddef.h>
#include <stdint.h>

namespace NETWORK {

/**
 * @brief Abstract HTTP client interface
 * 
 * Implementations should handle:
 * - Connection management (reuse, timeouts)
 * - Error handling and logging
 * - Thread safety if required
 */
class INetworkClient {
public:
    virtual ~INetworkClient() = default;

    /**
     * @brief Perform HTTP GET request
     * 
     * @param url Full URL to request
     * @param responseBuffer Optional buffer to store response body (can be nullptr)
     * @param bufferSize Size of response buffer
     * @param timeoutMs Timeout in milliseconds
     * @param bytesRead Optional output for actual bytes read
     * @return true on HTTP 200 success
     */
    virtual bool get(const char* url, 
                     char* responseBuffer = nullptr, 
                     size_t bufferSize = 0,
                     uint32_t timeoutMs = 10000, 
                     size_t* bytesRead = nullptr) = 0;

    /**
     * @brief Perform HTTP POST request
     * 
     * @param url Full URL to request
     * @param payload Request body
     * @param payloadLen Payload length in bytes
     * @param responseBuffer Optional buffer to store response body
     * @param bufferSize Size of response buffer
     * @param timeoutMs Timeout in milliseconds
     * @return true on HTTP 2xx success
     */
    virtual bool post(const char* url,
                      const char* payload,
                      size_t payloadLen,
                      char* responseBuffer = nullptr,
                      size_t bufferSize = 0,
                      uint32_t timeoutMs = 10000) = 0;
};

} // namespace NETWORK
