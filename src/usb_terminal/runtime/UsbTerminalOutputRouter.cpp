#include "UsbTerminalOutputRouter.h"

#include <cstdio>
#include <cstring>

#include <esp_heap_caps.h>

#include "../../system/logging/Logging.h"
#include "../UsbTerminalMessages.h"

#undef LOG_TAG
#define LOG_TAG "UsbTerminal"

namespace USB_TERMINAL {

UsbTerminalOutputRouter::UsbTerminalOutputRouter() {
    _mutex = xSemaphoreCreateMutex();
}

UsbTerminalOutputRouter::~UsbTerminalOutputRouter() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

void UsbTerminalOutputRouter::setTelegramMessageCallback(
    std::function<void(const char* targetId, const char* msg)> cb) {
    SYSTEM::ScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return;
    }
    _telegramMessageCallback = std::move(cb);
}

void UsbTerminalOutputRouter::setWebOutputCallback(
    std::function<void(const OutputEvent& event)> cb) {
    SYSTEM::ScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return;
    }
    _webOutputCallback = std::move(cb);
}

void UsbTerminalOutputRouter::setSessionChangeCallback(
    std::function<void(const SessionState& state)> cb) {
    SYSTEM::ScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return;
    }
    _sessionChangeCallback = std::move(cb);
}

void UsbTerminalOutputRouter::dispatchSessionChange(const SessionState& state) {
    std::function<void(const SessionState&)> callbackCopy;
    {
        SYSTEM::ScopeLock lock(_mutex);
        if (!lock.isLocked()) {
            return;
        }
        callbackCopy = _sessionChangeCallback;
    }

    if (callbackCopy) {
        callbackCopy(state);
    }
}

void UsbTerminalOutputRouter::dispatchTelegramMessage(const char* targetId, const char* message) {
    if (!targetId || !targetId[0] || !message || !message[0]) {
        return;
    }

    std::function<void(const char*, const char*)> callbackCopy;
    {
        SYSTEM::ScopeLock lock(_mutex);
        if (!lock.isLocked()) {
            return;
        }
        callbackCopy = _telegramMessageCallback;
    }

    if (callbackCopy) {
        callbackCopy(targetId, message);
    }
}

void UsbTerminalOutputRouter::dispatchTelegramOutput(const OutputEvent& event) {
    if (!event.targetId || !event.targetId[0] || !event.text || !event.text[0]) {
        return;
    }

    const size_t textLen = strlen(event.text);
    const char* title = telegramTitleForPhase(event.phase);
    const size_t capacity =
        textLen + LIMITS::USB_TERMINAL::MARKDOWN_WRAPPER_RESERVE_BYTES;
    char* message =
        static_cast<char*>(heap_caps_malloc(capacity, MALLOC_CAP_SPIRAM));
    if (!message) {
        LOGE("OOM: Failed to allocate Telegram terminal output message");
        return;
    }

    snprintf(message, capacity, "%s\n```text\n%s\n```", title, event.text);
    dispatchTelegramMessage(event.targetId, message);
    heap_caps_free(message);
}

void UsbTerminalOutputRouter::dispatchOutputEvent(
    SessionTransport transport,
    const char* targetId,
    OutputPhase phase,
    char* text) {
    if (!text || !text[0] || !targetId || !targetId[0]) {
        if (text) {
            heap_caps_free(text);
        }
        return;
    }

    if (transport == SessionTransport::Telegram) {
        OutputEvent event;
        event.transport = transport;
        event.phase = phase;
        event.targetId = targetId;
        event.text = text;
        dispatchTelegramOutput(event);
        heap_caps_free(text);
        return;
    }

    std::function<void(const OutputEvent&)> callbackCopy;
    {
        SYSTEM::ScopeLock lock(_mutex);
        if (!lock.isLocked()) {
            heap_caps_free(text);
            return;
        }
        callbackCopy = _webOutputCallback;
    }

    if (callbackCopy) {
        OutputEvent event;
        event.transport = transport;
        event.phase = phase;
        event.targetId = targetId;
        event.text = text;
        callbackCopy(event);
    }

    heap_caps_free(text);
}

}  // namespace USB_TERMINAL
