#include "UsbTerminalCapturePump.h"

#include <esp_heap_caps.h>

#include "../../system/utils/ScopeLock.h"
#include "../output/TerminalOutput.h"
#include "UsbTerminalSessionState.h"

namespace USB_TERMINAL {

CapturePumpResult UsbTerminalCapturePump::handleDisabled(
    SemaphoreHandle_t mutex,
    TerminalOutput& output,
    UsbTerminalSessionState& session) const {
    CapturePumpResult result{};
    bool hadActiveSession = false;

    SYSTEM::ScopeLock lock(mutex);
    if (!lock.isLocked()) {
        return result;
    }

    hadActiveSession = session.current().busy;
    output.end();
    session.clear();
    result.sessionSnapshot = session.copyState();
    result.shouldDispatchSessionChange = hadActiveSession;
    return result;
}

CapturePumpResult UsbTerminalCapturePump::process(
    SemaphoreHandle_t mutex,
    TerminalOutput& output,
    UsbTerminalSessionState& session,
    bool isShuttingDown,
    uint32_t idleTimeoutMs,
    const char* bytes,
    size_t bytesLen) const {
    CapturePumpResult result{};

    SYSTEM::ScopeLock lock(mutex);
    if (!lock.isLocked() || isShuttingDown) {
        return result;
    }

    if (!output.begin()) {
        return result;
    }

    FlushKind timeoutFlush = FlushKind::None;
    if (bytesLen == 0) {
        timeoutFlush =
            output.checkTimeouts(idleTimeoutMs, result.eventText, result.eventTargetId);
    }
    if (session.current().busy) {
        result.eventTransport = session.current().transport;
    }

    if (timeoutFlush == FlushKind::Partial) {
        result.eventPhase = OutputPhase::Partial;
    } else if (timeoutFlush == FlushKind::Final) {
        result.eventPhase = OutputPhase::Final;
        session.clear();
        result.shouldDispatchSessionChange = true;
        result.sessionSnapshot = session.copyState();
    }

    if (timeoutFlush == FlushKind::Final) {
        return result;
    }

    for (size_t i = 0; i < bytesLen; i++) {
        char* appendText = nullptr;
        char appendTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
        const FlushKind appendFlush =
            output.appendChar(bytes[i], appendText, appendTargetId);
        if (appendFlush == FlushKind::None) {
            continue;
        }

        if (!result.eventText) {
            result.eventText = appendText;
            strlcpy(result.eventTargetId, appendTargetId, sizeof(result.eventTargetId));
            if (session.current().busy) {
                result.eventTransport = session.current().transport;
            }
            result.eventPhase =
                appendFlush == FlushKind::Partial ? OutputPhase::Partial : OutputPhase::Final;
        } else if (appendText) {
            heap_caps_free(appendText);
        }

        if (appendFlush == FlushKind::Final) {
            session.clear();
            result.shouldDispatchSessionChange = true;
            result.sessionSnapshot = session.copyState();
        }
        break;
    }

    return result;
}

}  // namespace USB_TERMINAL
