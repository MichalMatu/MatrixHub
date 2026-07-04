#include "TerminalInput.h"
#include <esp_heap_caps.h>
#include <cstring>
#include <cctype>
#include <cstdio>
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "TerminalIn"

namespace USB_TERMINAL {

namespace {

constexpr const char* kAutoTargetPort = "auto";
constexpr const char* kAutoPortProbe =
    "p=\"$(for p in /dev/serial/by-id/*Espressif* /dev/serial/by-id/*ESP* "
    "/dev/serial/by-id/*esp* /dev/serial/by-id/*Waveshare* "
    "/dev/serial/by-id/*USB_JTAG* /dev/ttyACM* /dev/ttyUSB* "
    "/dev/cu.usbmodem* /dev/tty.usbmodem* /dev/cu.usbserial* /dev/tty.usbserial* "
    "/dev/serial/by-id/*; do [ -e \"$p\" ] && { printf '%s\\n' \"$p\"; break; }; done)\"; "
    "[ -n \"$p\" ] && ";

bool isAutoTargetPort(const char* targetPort) {
    return targetPort && strcasecmp(targetPort, kAutoTargetPort) == 0;
}

bool isStandaloneCdCommand(const char* cmdStart, size_t cmdLen) {
    if (!cmdStart || cmdLen < 2) {
        return false;
    }

    if (cmdStart[0] != 'c' || cmdStart[1] != 'd') {
        return false;
    }

    if (cmdLen > 2 && !isspace(static_cast<unsigned char>(cmdStart[2]))) {
        return false;
    }

    for (size_t i = 0; i < cmdLen; i++) {
        const char c = cmdStart[i];
        if (c == ';' || c == '|' || c == '<' || c == '>' || c == '\n' || c == '\r') {
            return false;
        }
        if (c == '&' && i + 1 < cmdLen && cmdStart[i + 1] == '&') {
            return false;
        }
    }

    return true;
}

} // namespace

TerminalInput::TerminalInput(KEYBOARD::KeyboardService* keyboardService) 
    : _keyboardService(keyboardService) {}

CommandType TerminalInput::parseCommand(const char* cmd, size_t& outCmdLen, const char*& outCmdStart) {
    if (!cmd) {
        outCmdLen = 0;
        outCmdStart = nullptr;
        return CommandType::NORMAL;
    }

    const char* start = cmd;
    size_t cmdLen = strlen(cmd);
    const char* end = start + cmdLen;

    // Trim leading whitespace
    while (start < end && isspace(static_cast<unsigned char>(*start))) {
        start++;
    }

    // Trim trailing whitespace
    while (end > start && isspace(static_cast<unsigned char>(*(end - 1)))) {
        end--;
    }

    outCmdLen = end - start;
    outCmdStart = start;

    if (outCmdLen == 2 && start[0] == '^' && (start[1] == 'C' || start[1] == 'c')) return CommandType::CTRL_C;
    if (outCmdLen == 6 && strncasecmp(start, "ctrl+c", 6) == 0) return CommandType::CTRL_C;
    if (outCmdLen == 6 && strncasecmp(start, "cancel", 6) == 0) return CommandType::CTRL_C;
    if (outCmdLen == 6 && strncasecmp(start, "status", 6) == 0) return CommandType::STATUS;

    return CommandType::NORMAL;
}

void TerminalInput::sendCtrlC() {
    if (!_keyboardService) return;
    
    LOGI("Sending Ctrl+C interrupt via keyboard");
    uint8_t keys[] = { 0x80, 'c' }; // 0x80 is KEY_LEFT_CTRL
    _keyboardService->pressCombo(keys, 2);
}

bool TerminalInput::sendCommand(const char* targetPort, const char* cmdStart, size_t cmdLen) {
    if (!_keyboardService || !cmdStart) return false;

    char* typedCommand = nullptr;
    
    if (!targetPort || targetPort[0] == '\0') {
        // No target port, just type the command blindly
        size_t requiredSize = cmdLen + 2; // +1 for '\n', +1 for '\0'
        typedCommand = (char*)heap_caps_malloc(requiredSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        
        if (typedCommand) {
            snprintf(typedCommand, requiredSize, "%.*s\n", (int)cmdLen, cmdStart);
        } else {
            LOGE("Failed to allocate typedCommand buffer in Internal RAM");
            return false;
        }
    } else if (isAutoTargetPort(targetPort)) {
        LOGI("Executing command with auto-detected target port (len=%u, content redacted)",
             static_cast<unsigned>(cmdLen));

        const bool wrapWithPwd = isStandaloneCdCommand(cmdStart, cmdLen);
        const char* format = wrapWithPwd
            ? "%s{ %.*s; pwd; } > \"$p\" 2>&1\n"
            : "%s%.*s > \"$p\" 2>&1\n";
        size_t requiredSize =
            snprintf(nullptr, 0, format, kAutoPortProbe, (int)cmdLen, cmdStart) + 1;
        typedCommand = (char*)heap_caps_malloc(requiredSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        if (typedCommand) {
            snprintf(typedCommand, requiredSize, format, kAutoPortProbe, (int)cmdLen, cmdStart);
        } else {
            LOGE("Failed to allocate typedCommand buffer in Internal RAM");
            return false;
        }
    } else {
        LOGI("Executing command on port %s (len=%u, content redacted)",
             targetPort,
             static_cast<unsigned>(cmdLen));

        const bool wrapWithPwd = isStandaloneCdCommand(cmdStart, cmdLen);
        const char* format = wrapWithPwd
            ? "{ %.*s; pwd; } > %s 2>&1\n"
            : "%.*s > %s 2>&1\n";
        size_t requiredSize = snprintf(nullptr, 0, format, (int)cmdLen, cmdStart, targetPort) + 1;
        typedCommand = (char*)heap_caps_malloc(requiredSize, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

        if (typedCommand) {
            snprintf(typedCommand, requiredSize, format, (int)cmdLen, cmdStart, targetPort);
        } else {
            LOGE("Failed to allocate typedCommand buffer in Internal RAM");
            return false;
        }
    }

    bool success = false;
    if (typedCommand) {
        _keyboardService->type(typedCommand);
        success = true;
        heap_caps_free(typedCommand);
    }
    
    return success;
}

} // namespace USB_TERMINAL
