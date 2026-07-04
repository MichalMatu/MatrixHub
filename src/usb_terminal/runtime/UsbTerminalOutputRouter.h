#pragma once

#include <functional>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "../../system/utils/ScopeLock.h"
#include "../UsbTerminalTypes.h"

namespace USB_TERMINAL {

class UsbTerminalOutputRouter {
public:
    UsbTerminalOutputRouter();
    ~UsbTerminalOutputRouter();

    void setTelegramMessageCallback(std::function<void(const char* targetId, const char* msg)> cb);
    void setWebOutputCallback(std::function<void(const OutputEvent& event)> cb);
    void setSessionChangeCallback(std::function<void(const SessionState& state)> cb);

    void dispatchSessionChange(const SessionState& state);
    void dispatchTelegramMessage(const char* targetId, const char* message);
    void dispatchOutputEvent(
        SessionTransport transport,
        const char* targetId,
        OutputPhase phase,
        char* text);

private:
    void dispatchTelegramOutput(const OutputEvent& event);

    SemaphoreHandle_t _mutex = nullptr;
    std::function<void(const char*, const char*)> _telegramMessageCallback;
    std::function<void(const OutputEvent& event)> _webOutputCallback;
    std::function<void(const SessionState& state)> _sessionChangeCallback;
};

}  // namespace USB_TERMINAL
