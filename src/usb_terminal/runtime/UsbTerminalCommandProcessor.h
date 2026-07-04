#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../../system/rtc/RtcConfig.h"
#include "../UsbTerminalTypes.h"

namespace USB_TERMINAL {

class TerminalInput;
class TerminalOutput;
class UsbTerminalSessionState;

struct CommandProcessingResult {
    CommandAck ack{};
    bool shouldDispatchSessionChange{false};
    SessionState sessionSnapshot{};
    SessionTransport eventTransport{SessionTransport::None};
    OutputPhase eventPhase{OutputPhase::Final};
    char eventTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN]{0};
    char* eventText{nullptr};
};

class UsbTerminalCommandProcessor {
public:
    CommandProcessingResult execute(
        SemaphoreHandle_t mutex,
        TerminalInput& input,
        TerminalOutput& output,
        UsbTerminalSessionState& session,
        const RTC::UsbTerminalData& cfg,
        const char* cmd,
        SessionTransport transport,
        const char* targetId) const;

    bool handleDisconnect(
        SemaphoreHandle_t mutex,
        TerminalInput& input,
        TerminalOutput& output,
        UsbTerminalSessionState& session,
        SessionTransport transport,
        const char* targetId,
        SessionState& sessionSnapshot) const;

private:
    static void setAck(CommandAck& ack, bool ok, const char* message);
};

}  // namespace USB_TERMINAL
