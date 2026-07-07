#include "UsbTerminalCapturePump.h"

#include <cstring>
#include <esp_heap_caps.h>

#include "../../system/utils/ScopeLock.h"
#include "../output/TerminalOutput.h"
#include "UsbTerminalSessionState.h"

namespace USB_TERMINAL {

namespace {

void appendPendingBytes(char* pending, size_t& pendingLen, size_t pendingSize, const char* bytes, size_t bytesLen) {
    if (!pending || pendingSize == 0 || !bytes || bytesLen == 0) {
        return;
    }
    if (pendingLen >= pendingSize) {
        return;
    }

    const size_t available = pendingSize - pendingLen;
    const size_t copyLen = bytesLen < available ? bytesLen : available;
    if (copyLen > 0) {
        memcpy(pending + pendingLen, bytes, copyLen);
        pendingLen += copyLen;
    }
}

}  // namespace

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
    _pendingLen = 0;
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
    if (!lock.isLocked()) {
        return result;
    }

    if (isShuttingDown) {
        _pendingLen = 0;
        return result;
    }

    if (!output.begin()) {
        return result;
    }

    FlushKind timeoutFlush = FlushKind::None;
    if (_pendingLen == 0 && bytesLen == 0) {
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

    auto recordFlush = [&](FlushKind appendFlush, char* appendText, const char* appendTargetId) {
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
            _pendingLen = 0;
            result.shouldDispatchSessionChange = true;
            result.sessionSnapshot = session.copyState();
        }
    };

    auto processBytes = [&](const char* source, size_t len, bool sourceIsPending) {
        for (size_t i = 0; i < len; i++) {
            char* appendText = nullptr;
            char appendTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
            const FlushKind appendFlush =
                output.appendChar(source[i], appendText, appendTargetId);
            if (appendFlush == FlushKind::None) {
                continue;
            }

            recordFlush(appendFlush, appendText, appendTargetId);

            if (appendFlush == FlushKind::Final) {
                return true;
            }

            if (sourceIsPending) {
                const size_t remainingLen = len - i - 1;
                if (remainingLen > 0) {
                    memmove(_pendingBytes, source + i + 1, remainingLen);
                }
                _pendingLen = remainingLen;
                appendPendingBytes(
                    _pendingBytes,
                    _pendingLen,
                    kPendingBufferSize,
                    bytes,
                    bytesLen);
            } else {
                _pendingLen = 0;
                appendPendingBytes(
                    _pendingBytes,
                    _pendingLen,
                    kPendingBufferSize,
                    source + i + 1,
                    len - i - 1);
            }
            return true;
        }

        if (sourceIsPending) {
            _pendingLen = 0;
        }
        return false;
    };

    if (_pendingLen > 0 && processBytes(_pendingBytes, _pendingLen, true)) {
        return result;
    }

    if (bytesLen > 0 && processBytes(bytes, bytesLen, false)) {
        return result;
    }

    return result;
}

}  // namespace USB_TERMINAL
