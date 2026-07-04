#include "UsbTerminalCommandProcessor.h"

#include <cstring>
#include <esp_heap_caps.h>

#include "../../system/utils/ScopeLock.h"
#include "../UsbTerminalMessages.h"
#include "../input/TerminalInput.h"
#include "../output/TerminalOutput.h"
#include "UsbTerminalSessionState.h"

namespace USB_TERMINAL {

void UsbTerminalCommandProcessor::setAck(CommandAck& ack, bool ok, const char* message) {
    ack.ok = ok;
    if (message) {
        strlcpy(ack.message, message, sizeof(ack.message));
    } else {
        ack.message[0] = '\0';
    }
}

CommandProcessingResult UsbTerminalCommandProcessor::execute(
    SemaphoreHandle_t mutex,
    TerminalInput& input,
    TerminalOutput& output,
    UsbTerminalSessionState& session,
    const RTC::UsbTerminalData& cfg,
    const char* cmd,
    SessionTransport transport,
    const char* targetId) const {
    CommandProcessingResult result{};

    size_t cmdLen = 0;
    const char* cmdStart = nullptr;
    const CommandType type = input.parseCommand(cmd, cmdLen, cmdStart);

    char safeTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
    if (targetId) {
        strlcpy(safeTargetId, targetId, sizeof(safeTargetId));
    }

    if (type == CommandType::CTRL_C) {
        bool shouldInterrupt = false;

        {
            SYSTEM::ScopeLock lock(mutex);
            if (!lock.isLocked()) {
                setAck(result.ack, false, kMsgInternalBuffer);
            } else if (!session.current().busy) {
                setAck(result.ack, false, kMsgIdle);
            } else if (!session.isOwner(transport, safeTargetId)) {
                setAck(result.ack, false, kMsgBusyOtherSession);
            } else {
                shouldInterrupt = true;
                setAck(result.ack, true, kMsgInterrupted);
            }
        }

        if (shouldInterrupt) {
            input.sendCtrlC();

            SYSTEM::ScopeLock lock(mutex);
            if (lock.isLocked()) {
                result.eventTransport = session.current().transport;
                result.eventPhase = OutputPhase::Interrupted;
                output.stopCapture(result.eventText, result.eventTargetId);
                session.clear();
                result.shouldDispatchSessionChange = true;
                result.sessionSnapshot = session.copyState();
            }
        }

        return result;
    }

    if (type == CommandType::STATUS) {
        SYSTEM::ScopeLock lock(mutex);
        if (!lock.isLocked()) {
            setAck(result.ack, false, kMsgInternalBuffer);
        } else if (!session.current().busy) {
            setAck(result.ack, true, kMsgIdle);
        } else if (!session.isOwner(transport, safeTargetId)) {
            setAck(result.ack, false, kMsgBusyOtherSession);
        } else {
            result.eventTransport = session.current().transport;
            result.eventPhase = OutputPhase::Status;
            if (output.gatherStatus(result.eventText)) {
                strlcpy(
                    result.eventTargetId,
                    session.current().targetId,
                    sizeof(result.eventTargetId));
                setAck(result.ack, true, kMsgRunning);
            } else {
                setAck(result.ack, true, kMsgRunningNoOutput);
            }
        }
        return result;
    }

    if (!cfg.enabled) {
        setAck(result.ack, false, kMsgDisabled);
        return result;
    }

    bool doExecute = false;
    char safePort[sizeof(RTC::UsbTerminalData::targetPort)] = {0};

    {
        SYSTEM::ScopeLock lock(mutex);
        if (!lock.isLocked()) {
            setAck(result.ack, false, kMsgInternalBuffer);
            return result;
        }

        if (session.current().busy) {
            setAck(
                result.ack,
                false,
                session.isOwner(transport, safeTargetId)
                    ? kMsgBusyOwnSession
                    : kMsgBusyOtherSession);
            return result;
        }

        if (!output.begin()) {
            setAck(result.ack, false, kMsgInternalBuffer);
            return result;
        }

        doExecute = true;
        memcpy(safePort, cfg.targetPort, sizeof(safePort));
        safePort[sizeof(safePort) - 1] = '\0';

        output.startCapture(safeTargetId);
        session.setActive(transport, safeTargetId);
        result.shouldDispatchSessionChange = true;
        result.sessionSnapshot = session.copyState();

        setAck(result.ack, true, safePort[0] == '\0' ? kMsgPortNotSet : nullptr);
    }

    if (!doExecute || input.sendCommand(safePort, cmdStart, cmdLen)) {
        return result;
    }

    SYSTEM::ScopeLock lock(mutex);
    if (lock.isLocked()) {
        result.eventTransport = session.current().transport;
        output.stopCapture(result.eventText, result.eventTargetId);
        session.clear();
        result.shouldDispatchSessionChange = true;
        result.sessionSnapshot = session.copyState();
    }

    if (result.eventText) {
        heap_caps_free(result.eventText);
        result.eventText = nullptr;
    }
    setAck(result.ack, false, kMsgInternalCommand);
    return result;
}

bool UsbTerminalCommandProcessor::handleDisconnect(
    SemaphoreHandle_t mutex,
    TerminalInput& input,
    TerminalOutput& output,
    UsbTerminalSessionState& session,
    SessionTransport transport,
    const char* targetId,
    SessionState& sessionSnapshot) const {
    char safeTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
    if (targetId) {
        strlcpy(safeTargetId, targetId, sizeof(safeTargetId));
    }

    {
        SYSTEM::ScopeLock lock(mutex);
        if (!lock.isLocked() || !session.current().busy ||
            !session.isOwner(transport, safeTargetId)) {
            return false;
        }
    }

    input.sendCtrlC();

    SYSTEM::ScopeLock lock(mutex);
    if (!lock.isLocked()) {
        return false;
    }

    char* discardedText = nullptr;
    char discardedTargetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN] = {0};
    output.stopCapture(discardedText, discardedTargetId);
    if (discardedText) {
        heap_caps_free(discardedText);
    }

    session.clear();
    sessionSnapshot = session.copyState();
    return true;
}

}  // namespace USB_TERMINAL
