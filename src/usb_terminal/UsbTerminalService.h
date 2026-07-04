#pragma once

#include <Arduino.h>
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../keyboard/KeyboardService.h"
#include "../config/System.h"
#include "UsbTerminalTypes.h"
#include "input/TerminalInput.h"
#include "output/TerminalOutput.h"
#include "runtime/UsbTerminalCapturePump.h"
#include "runtime/UsbTerminalCommandProcessor.h"
#include "runtime/UsbTerminalOutputRouter.h"
#include "runtime/UsbTerminalSessionState.h"

namespace USB_TERMINAL {

/**
 * @class UsbTerminalService
 * @brief Provides a remote terminal connection by typing commands over USB CDC
 * and capturing responses via the Serial port.
 */
class UsbTerminalService {
public:
    UsbTerminalService(KEYBOARD::KeyboardService* keyboardService);
    ~UsbTerminalService();

    void begin();
    void loop();

    /**
     * Injects transport sinks for Telegram and WebSocket consumers.
     */
    void setTelegramMessageCallback(std::function<void(const char* targetId, const char* msg)> cb);
    void setWebOutputCallback(std::function<void(const OutputEvent& event)> cb);
    void setSessionChangeCallback(std::function<void(const SessionState& state)> cb);

    /**
     * Execute a terminal request from the given transport owner.
     */
    CommandAck execute(const char* cmd, SessionTransport transport, const char* targetId);

    /**
     * Return a session snapshot tailored for the requester.
     */
    SessionSnapshot getSessionSnapshot(SessionTransport requesterTransport, const char* requesterId);

    /**
     * Auto-cancel an active session when its owner disconnects.
     */
    bool handleDisconnect(SessionTransport transport, const char* targetId);

private:
    SemaphoreHandle_t _mutex;
    
    TerminalInput _input;
    TerminalOutput _output;
    UsbTerminalSessionState _session;
    UsbTerminalOutputRouter _router;
    UsbTerminalCommandProcessor _commandProcessor;
    UsbTerminalCapturePump _capturePump;
    
    bool _isShuttingDown{false};
};

} // namespace USB_TERMINAL
