#include "UsbTerminalService.h"

#include <USB.h>
#include <USBCDC.h>
#include <cstdio>
#include <cstring>

#include "../system/logging/Logging.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/utils/ScopeLock.h"
#include "UsbTerminalMessages.h"

#undef LOG_TAG
#define LOG_TAG "UsbTerminal"

// Use existing USBSerial from LogOutput.cpp or aliased Serial
#if ARDUINO_USB_CDC_ON_BOOT
#define USB_PORT Serial
#else
extern USBCDC USBSerial;
#define USB_PORT USBSerial
#endif

namespace USB_TERMINAL {

UsbTerminalService::UsbTerminalService(KEYBOARD::KeyboardService* keyboardService)
    : _input(keyboardService) {
    _mutex = xSemaphoreCreateMutex();
}

UsbTerminalService::~UsbTerminalService() {
    _isShuttingDown = true;

    // Give other tasks a moment to see the flag and exit their critical sections
    vTaskDelay(pdMS_TO_TICKS(50));

    // Leaving _mutex untouched because deleting it while tasks wait causes kernel panics.
}

void UsbTerminalService::begin() {
    LOGI("Initializing USB Terminal Service...");

    SYSTEM::ScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return;
    }

#if !ARDUINO_USB_CDC_ON_BOOT
    USB_PORT.begin();
#endif
}

void UsbTerminalService::setTelegramMessageCallback(std::function<void(const char*, const char*)> cb) {
    _router.setTelegramMessageCallback(std::move(cb));
}

void UsbTerminalService::setWebOutputCallback(std::function<void(const OutputEvent& event)> cb) {
    _router.setWebOutputCallback(std::move(cb));
}

void UsbTerminalService::setSessionChangeCallback(std::function<void(const SessionState& state)> cb) {
    _router.setSessionChangeCallback(std::move(cb));
}

CommandAck UsbTerminalService::execute(const char* cmd, SessionTransport transport, const char* targetId) {
    RTC::UsbTerminalData cfg{};
    RTC::withConfig([&](const RTC::ConfigStore& c) { cfg = c.usbTerminal; });

    const CommandProcessingResult result =
        _commandProcessor.execute(_mutex, _input, _output, _session, cfg, cmd, transport, targetId);

    if (result.eventText) {
        _router.dispatchOutputEvent(
            result.eventTransport,
            result.eventTargetId,
            result.eventPhase,
            result.eventText);
    }

    if (result.shouldDispatchSessionChange) {
        _router.dispatchSessionChange(result.sessionSnapshot);
    }

    if (transport == SessionTransport::Telegram && result.ack.message[0] != '\0') {
        _router.dispatchTelegramMessage(targetId, result.ack.message);
    }

    return result.ack;
}

SessionSnapshot UsbTerminalService::getSessionSnapshot(SessionTransport requesterTransport, const char* requesterId) {
    SYSTEM::ScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return SessionSnapshot{};
    }

    return _session.snapshotFor(requesterTransport, requesterId);
}

bool UsbTerminalService::handleDisconnect(SessionTransport transport, const char* targetId) {
    SessionState sessionSnapshot{};
    if (!_commandProcessor.handleDisconnect(
            _mutex,
            _input,
            _output,
            _session,
            transport,
            targetId,
            sessionSnapshot)) {
        return false;
    }

    _router.dispatchSessionChange(sessionSnapshot);
    return true;
}

void UsbTerminalService::loop() {
    RTC::UsbTerminalData cfg{};
    RTC::withConfig([&](const RTC::ConfigStore& c) { cfg = c.usbTerminal; });

    if (!cfg.enabled) {
        const CapturePumpResult result =
            _capturePump.handleDisabled(_mutex, _output, _session);
        if (result.shouldDispatchSessionChange) {
            _router.dispatchSessionChange(result.sessionSnapshot);
        }
        return;
    }

    char localBuf[256];
    size_t localRead = 0;

    while (USB_PORT.available() && localRead < sizeof(localBuf)) {
        localBuf[localRead++] = static_cast<char>(USB_PORT.read());
    }

    const CapturePumpResult result = _capturePump.process(
        _mutex,
        _output,
        _session,
        _isShuttingDown,
        cfg.idleTimeoutMs,
        localBuf,
        localRead);

    if (result.eventText) {
        _router.dispatchOutputEvent(
            result.eventTransport,
            result.eventTargetId,
            result.eventPhase,
            result.eventText);
    }

    if (result.shouldDispatchSessionChange) {
        _router.dispatchSessionChange(result.sessionSnapshot);
    }
}

} // namespace USB_TERMINAL
