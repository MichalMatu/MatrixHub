#pragma once

#include <cstddef>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../UsbTerminalTypes.h"

namespace USB_TERMINAL {

class TerminalOutput;
class UsbTerminalSessionState;

struct CapturePumpResult {
    bool shouldDispatchSessionChange{false};
    SessionState sessionSnapshot{};
    SessionTransport eventTransport{SessionTransport::None};
    OutputPhase eventPhase{OutputPhase::Final};
    char eventTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN]{0};
    char* eventText{nullptr};
};

class UsbTerminalCapturePump {
public:
    CapturePumpResult handleDisabled(
        SemaphoreHandle_t mutex,
        TerminalOutput& output,
        UsbTerminalSessionState& session) const;

    CapturePumpResult process(
        SemaphoreHandle_t mutex,
        TerminalOutput& output,
        UsbTerminalSessionState& session,
        bool isShuttingDown,
        uint32_t idleTimeoutMs,
        const char* bytes,
        size_t bytesLen) const;

private:
    static constexpr size_t kPendingBufferSize =
        LIMITS::USB_TERMINAL::MAX_OUTPUT_BUFFER_SIZE;

    mutable char _pendingBytes[kPendingBufferSize]{0};
    mutable size_t _pendingLen{0};
};

}  // namespace USB_TERMINAL
