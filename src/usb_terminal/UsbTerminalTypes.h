#pragma once

#include <cstdint>
#include "../config/System.h"

namespace USB_TERMINAL {

enum class SessionTransport : uint8_t {
    None = 0,
    Telegram,
    Web
};

enum class OutputPhase : uint8_t {
    Partial = 0,
    Final,
    Interrupted,
    Status
};

enum class FlushKind : uint8_t {
    None = 0,
    Partial,
    Final
};

struct SessionState {
    bool busy{false};
    SessionTransport transport{SessionTransport::None};
    char targetId[LIMITS::USB_TERMINAL::MAX_TARGET_ID_LEN]{0};
};

struct SessionSnapshot {
    bool busy{false};
    bool owner{false};
    SessionTransport transport{SessionTransport::None};
};

struct OutputEvent {
    SessionTransport transport{SessionTransport::None};
    OutputPhase phase{OutputPhase::Final};
    const char* targetId{nullptr};
    const char* text{nullptr};
};

struct CommandAck {
    bool ok{false};
    char message[LIMITS::USB_TERMINAL::MAX_STATUS_MESSAGE_LEN]{0};
};

inline const char* toTransportString(SessionTransport transport) {
    switch (transport) {
        case SessionTransport::Telegram: return "telegram";
        case SessionTransport::Web: return "ws";
        case SessionTransport::None:
        default: return nullptr;
    }
}

inline const char* toPhaseString(OutputPhase phase) {
    switch (phase) {
        case OutputPhase::Partial: return "partial";
        case OutputPhase::Final: return "final";
        case OutputPhase::Interrupted: return "interrupted";
        case OutputPhase::Status:
        default: return "status";
    }
}

} // namespace USB_TERMINAL
