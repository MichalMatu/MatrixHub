#pragma once

#include <cstring>

#include "../UsbTerminalTypes.h"

namespace USB_TERMINAL {

class UsbTerminalSessionState {
public:
    bool isOwner(SessionTransport transport, const char* targetId) const {
        if (!_activeSession.busy || _activeSession.transport != transport) {
            return false;
        }

        const char* safeTargetId = targetId ? targetId : "";
        return strncmp(_activeSession.targetId, safeTargetId, sizeof(_activeSession.targetId)) == 0;
    }

    void setActive(SessionTransport transport, const char* targetId) {
        _activeSession.busy = true;
        _activeSession.transport = transport;
        strlcpy(
            _activeSession.targetId,
            targetId ? targetId : "",
            sizeof(_activeSession.targetId));
    }

    void clear() {
        _activeSession.busy = false;
        _activeSession.transport = SessionTransport::None;
        _activeSession.targetId[0] = '\0';
    }

    SessionState copyState() const {
        return _activeSession;
    }

    SessionSnapshot snapshotFor(SessionTransport requesterTransport, const char* requesterId) const {
        SessionSnapshot snapshot{};
        snapshot.busy = _activeSession.busy;
        snapshot.transport = _activeSession.transport;
        snapshot.owner = isOwner(requesterTransport, requesterId);
        return snapshot;
    }

    const SessionState& current() const {
        return _activeSession;
    }

private:
    SessionState _activeSession{};
};

}  // namespace USB_TERMINAL
