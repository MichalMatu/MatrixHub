#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

namespace SYSTEM {
namespace HEALTH {

struct HttpServerHealthSnapshot {
    // These counters describe transport-level sockets accepted by the HTTP(S)
    // server. A WebSocket upgrade reuses the same socket, so WS handshakes and
    // channel subscribe/unsubscribe traffic must not change these values.
    uint32_t activeClients = 0;
    uint32_t peakClients = 0;
    uint32_t openCount = 0;
    uint32_t closeCount = 0;
    uint32_t lastOpenMs = 0;
    uint32_t lastCloseMs = 0;
    // WebSocket counters describe registered WS sessions inside WsClientManager.
    // They are intentionally separate from HTTP transport counters because a
    // WS upgrade reuses one accepted socket and can outlive many REST polls.
    uint32_t wsActiveClients = 0;
    uint32_t wsPeakClients = 0;
    uint32_t wsOpenCount = 0;
    uint32_t wsCloseCount = 0;
    uint32_t lastWsOpenMs = 0;
    uint32_t lastWsCloseMs = 0;
    uint32_t wsForcedRemovals = 0;
    uint32_t wsQueueDrops = 0;
    uint32_t lastWsQueueDropMs = 0;
    uint32_t lastWsQueueDropPayload = 0;
    // Heap fallback counters track allocation sites, not logical messages.
    // A single large snapshot can therefore increment this more than once
    // (for example: serialization buffer fallback plus queued broadcast copy).
    uint32_t wsHeapFallbacks = 0;
    uint32_t lastWsHeapFallbackMs = 0;
    uint32_t lastWsHeapFallbackPayload = 0;
    uint32_t maxWsHeapFallbackPayload = 0;
};

class HttpServerHealthTracker {
public:
    static void reset();
    static void recordOpen();
    static void recordClose();
    static void recordWsOpen();
    static void recordWsClose();
    static void recordWsForcedRemoval(int fd);
    static void recordWsQueueDrop(size_t payloadLen = 0);
    // Record that the WS path had to leave the fixed-buffer fast path and fall
    // back to heap allocation for this payload size.
    static void recordWsHeapFallback(size_t payloadLen = 0);
    static HttpServerHealthSnapshot getSnapshot();

private:
    static HttpServerHealthSnapshot _snapshot;
    static uint32_t _recentCloseWindowStartMs;
    static uint8_t _recentCloseBurstCount;
};

}  // namespace HEALTH
}  // namespace SYSTEM
