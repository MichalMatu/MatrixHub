#pragma once

#include <cstdint>

namespace API {

struct CsiCaptureCleanupRequest {
    uint32_t requestGeneration = 0;
    int fd = -1;
    uint32_t sessionGeneration = 0;
};

enum class CsiCaptureCleanupResolution : uint8_t {
    Retry,
    ReleaseOwnedSession,
    EnsureInactive,
    Obsolete,
};

/**
 * Owner-scoped mailbox for a capture disconnect that could not acquire the
 * session mutex.
 *
 * The API has at most one active capture session.  The socket descriptor alone
 * is not a stable ownership token because lwIP may reuse it; sessionGeneration
 * prevents a delayed cleanup from releasing a newer session on the same fd.
 * The helper is intentionally platform-agnostic and pure, but not internally
 * synchronized. WifiSensingApiService serializes access with its deferred
 * schedule critical section.
 */
class CsiCaptureCleanupGate {
public:
    static bool ownsSession(int fd,
                            uint32_t sessionGeneration,
                            int activeFd,
                            uint32_t activeSessionGeneration) {
        return fd >= 0 && fd == activeFd &&
               sessionGeneration == activeSessionGeneration;
    }

    /**
     * Publish cleanup only when the request still names the active owner.
     *
     * A cleanup callback from any other WebSocket must never overwrite the
     * only durable disconnect notification for the capture owner. Repeated
     * callbacks for the same owner are idempotent; a later session can replace
     * an older pending request only after it becomes the active owner.
     *
     * @return Non-zero request generation when accepted, otherwise zero.
     */
    uint32_t requestIfOwned(int fd,
                            uint32_t sessionGeneration,
                            int activeFd,
                            uint32_t activeSessionGeneration) {
        if (!ownsSession(
                fd, sessionGeneration, activeFd, activeSessionGeneration)) {
            return 0;
        }
        if (hasPendingRequest() &&
            _request.fd == fd &&
            _request.sessionGeneration == sessionGeneration) {
            return _request.requestGeneration;
        }

        ++_requestedGeneration;
        if (_requestedGeneration == 0) {
            ++_requestedGeneration;
        }
        _request = {_requestedGeneration, fd, sessionGeneration};
        return _requestedGeneration;
    }

    bool hasPendingRequest() const {
        return _requestedGeneration != _completedGeneration;
    }

    bool snapshot(CsiCaptureCleanupRequest& out) const {
        if (!hasPendingRequest()) {
            return false;
        }
        out = _request;
        return true;
    }

    bool completeIfCurrent(uint32_t requestGeneration) {
        if (!hasPendingRequest() ||
            _requestedGeneration != requestGeneration) {
            return false;
        }
        _completedGeneration = requestGeneration;
        return true;
    }

    uint32_t requestedGeneration() const {
        return _requestedGeneration;
    }

    uint32_t completedGeneration() const {
        return _completedGeneration;
    }

    static CsiCaptureCleanupResolution resolve(
        const CsiCaptureCleanupRequest& request,
        bool sessionLockAcquired,
        int activeFd,
        uint32_t activeSessionGeneration) {
        if (!sessionLockAcquired) {
            return CsiCaptureCleanupResolution::Retry;
        }
        if (activeSessionGeneration != request.sessionGeneration) {
            return CsiCaptureCleanupResolution::Obsolete;
        }
        if (activeFd < 0) {
            return CsiCaptureCleanupResolution::EnsureInactive;
        }
        if (activeFd == request.fd) {
            return CsiCaptureCleanupResolution::ReleaseOwnedSession;
        }
        return CsiCaptureCleanupResolution::Obsolete;
    }

private:
    uint32_t _requestedGeneration = 0;
    uint32_t _completedGeneration = 0;
    CsiCaptureCleanupRequest _request;
};

} // namespace API
