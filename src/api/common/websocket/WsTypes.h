#pragma once

#include <cstdint>
#include <esp_http_server.h>

namespace API {
namespace WEBSOCKET {

constexpr size_t MAX_BROADCAST_TARGETS = 32;
constexpr size_t INLINE_PAYLOAD_CAPACITY = 16;
using WsClientGeneration = uint32_t;
constexpr WsClientGeneration INVALID_CLIENT_GENERATION = 0;

struct WsMessage {
    uint8_t* data;
    size_t len;
    httpd_ws_type_t type;
    bool isAllocated;
    int16_t payloadSlot;
    int targets[MAX_BROADCAST_TARGETS]; // Specific FDs to target (0 = all)
    size_t targetCount;
    // A numeric fd may be reused by a later websocket session while this
    // message waits in the async queue. Keep the handshake generation with
    // every targeted fd so stale payloads cannot cross that session boundary.
    WsClientGeneration targetGenerations[MAX_BROADCAST_TARGETS];
    uint8_t inlineData[INLINE_PAYLOAD_CAPACITY];
    bool isInline;
};

} // namespace WEBSOCKET
} // namespace API
