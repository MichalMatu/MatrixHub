#include "SystemSnapshots.h"

#include <ArduinoJson.h>

#include "../../../../system/logging/Logging.h"

#include <algorithm>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "SysSnapTx"

namespace API::SYSTEM_WS {

namespace {

class SnapshotBufferWriter {
public:
    SnapshotBufferWriter(uint8_t* buffer, size_t capacity)
        : _buffer(buffer), _capacity(capacity) {}

    size_t write(uint8_t c) {
        if (_pos < _capacity) {
            _buffer[_pos] = c;
        }
        _pos++;
        return 1;
    }

    size_t write(const uint8_t* data, size_t len) {
        if (data && len > 0 && _pos < _capacity) {
            const size_t writable = std::min(len, _capacity - _pos);
            memcpy(_buffer + _pos, data, writable);
        }
        _pos += len;
        return len;
    }

private:
    uint8_t* _buffer;
    size_t _capacity;
    size_t _pos = 0;
};

}  // namespace

bool sendSnapshotDoc(WebSocketBroadcaster* ws,
                     int fd,
                     SYSTEM::SpiRamJsonDocument& doc) {
    if (!ws) {
        return false;
    }

    // Fail closed if the JSON document overflowed while being assembled.
    // Sending a syntactically valid but truncated snapshot is worse than
    // dropping it here because the frontend would treat partial state as
    // authoritative and render inconsistent diagnostics/UI.
    if (doc.overflowed()) {
        const char* channel = doc["channel"].is<const char*>() ? doc["channel"].as<const char*>() : "unknown";
        LOGE("Snapshot JSON overflow for channel '%s'; dropping payload", channel);
        return false;
    }

    const size_t reserveLen = doc.requestedCapacityLimit();
    if (reserveLen == 0) {
        return false;
    }

    int target = fd;
    size_t requiredLen = 0;
    const bool sent = ws->broadcastSerialized(
        &target,
        1,
        reserveLen,
        [&doc, &requiredLen](uint8_t* buffer, size_t capacity) -> size_t {
            SnapshotBufferWriter writer(buffer, capacity);
            requiredLen = serializeJson(doc, writer);
            return requiredLen;
        },
        HTTPD_WS_TYPE_TEXT);
    if (!sent && requiredLen > reserveLen) {
        const char* channel = doc["channel"].is<const char*>() ? doc["channel"].as<const char*>() : "unknown";
        LOGE("Snapshot JSON exceeded reserve for channel '%s': %u > %u bytes",
             channel,
             static_cast<unsigned>(requiredLen),
             static_cast<unsigned>(reserveLen));
    }
    return sent;
}

}  // namespace API::SYSTEM_WS
