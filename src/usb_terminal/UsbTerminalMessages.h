#pragma once

#include "UsbTerminalTypes.h"

namespace USB_TERMINAL {

inline constexpr const char* kMsgDisabled =
    "USB Terminal Service is disabled. Enable it in the Web UI.";
inline constexpr const char* kMsgBusyOtherSession =
    "Terminal is busy with another session.";
inline constexpr const char* kMsgBusyOwnSession =
    "Terminal is busy processing the previous command. Use status or cancel.";
inline constexpr const char* kMsgInternalBuffer =
    "Internal error: failed to allocate terminal output buffer.";
inline constexpr const char* kMsgInternalCommand =
    "Internal error: failed to type the command.";
inline constexpr const char* kMsgPortNotSet =
    "Port not set. Typing blindly without feedback.";
inline constexpr const char* kMsgIdle = "Terminal is idle.";
inline constexpr const char* kMsgRunning = "Command is still running.";
inline constexpr const char* kMsgRunningNoOutput =
    "Command is running, but no output has been captured yet.";
inline constexpr const char* kMsgInterrupted =
    "Interrupt signal (Ctrl+C) sent to host.";

inline const char* telegramTitleForPhase(OutputPhase phase) {
    switch (phase) {
        case OutputPhase::Partial:
            return "Output (Part):";
        case OutputPhase::Final:
            return "Command Output:";
        case OutputPhase::Interrupted:
            return "Command Interrupted. Captured so far:";
        case OutputPhase::Status:
        default:
            return "Current Capture Buffer:";
    }
}

}  // namespace USB_TERMINAL
