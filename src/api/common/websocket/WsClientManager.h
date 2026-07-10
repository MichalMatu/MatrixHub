#pragma once

#include <map>
#include <atomic>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_http_server.h>
#include "WsTypes.h"
#include "../../../system/memory/PsramAllocator.h"

namespace API {
class IWebSocketAuthenticator;

namespace WEBSOCKET {

class WsClientManager {
public:
    using StateChangeCallback = std::function<void(bool hasClients)>;
    static constexpr uint8_t MAX_SEND_FAILURES = 20;

    WsClientManager(const char* logTag, 
                    IWebSocketAuthenticator* authenticator, 
                    StateChangeCallback onStateChange, 
                    uint32_t sendTimeoutMs);
    ~WsClientManager();

    esp_err_t handleHandshake(httpd_req_t *req);
    void removeClient(
        int fd,
        bool triggerClose = false,
        bool isBroadcastTaskContext = false,
        WsClientGeneration expectedGeneration = INVALID_CLIENT_GENERATION);
    
    // Broadcast synchronously to connected clients
    void performBroadcast(
        uint8_t* data,
        size_t len,
        httpd_ws_type_t type,
        int* targets = nullptr,
        size_t targetCount = 0,
        bool isBroadcastTaskContext = false,
        const WsClientGeneration* targetGenerations = nullptr);
    
    // State management for async disconnect handling
    void drainPendingDisconnect(bool isBroadcastTaskContext);
    void setDisconnectPending(bool pending);

    bool hasClients() const;
    size_t getClientCount() const;
    httpd_handle_t getServerHandle() const;
    size_t snapshotClients(int* outTargets, size_t maxCount) const;
    size_t snapshotTargetSessions(const int* requestedTargets,
                                  size_t requestedCount,
                                  int* outTargets,
                                  WsClientGeneration* outGenerations,
                                  size_t maxCount) const;
    // A client that has sent a valid application-level START command no longer
    // needs the generic post-handshake grace period before targeted replies.
    bool markClientReady(int fd);

private:
    struct ClientState {
        uint8_t failures = 0;
        uint32_t readyAtMs = 0;
        WsClientGeneration generation = INVALID_CLIENT_GENERATION;
    };

    static bool isReadyForBroadcast(const ClientState& client, uint32_t now);

    const char* _logTag;
    IWebSocketAuthenticator* _authenticator;
    StateChangeCallback _onStateChange;
    uint32_t _sendTimeoutMs;
    httpd_handle_t _serverHandle = nullptr; 
    SemaphoreHandle_t _lock = nullptr;
    std::atomic<bool> _disconnectCallbackPending{false};
    std::atomic<size_t> _clientCount{0};
    WsClientGeneration _lastClientGeneration = INVALID_CLIENT_GENERATION;

    using PsramClientMap = std::map<int, ClientState, std::less<int>, SYSTEM::PsramAllocator<std::pair<const int, ClientState>>>;
    PsramClientMap _clients;
};

} // namespace WEBSOCKET
} // namespace API
