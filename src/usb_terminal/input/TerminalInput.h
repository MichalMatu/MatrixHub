#pragma once

#include <cstdint>
#include <cstddef>
#include "../../keyboard/KeyboardService.h"
#include "../../config/System.h"

namespace USB_TERMINAL {

enum class CommandType {
    NORMAL,
    CTRL_C,
    STATUS
};

/**
 * @class TerminalInput
 * @brief Parses commands and forms keyboard injections.
 */
class TerminalInput {
public:
    TerminalInput(KEYBOARD::KeyboardService* keyboardService);

    /**
     * @brief Parses the raw command string to determine its intent.
     * @param cmd Raw command.
     * @param outCmdLen Parsed command length after trimming.
     * @param outCmdStart Pointer to the start of the trimmed command.
     * @return The determined CommandType.
     */
    CommandType parseCommand(const char* cmd, size_t& outCmdLen, const char*& outCmdStart);

    /**
     * @brief Injects Ctrl+C into the keyboard stream.
     */
    void sendCtrlC();

    /**
     * @brief Forms the command with redirection and injects it.
     * @param targetPort The serial port path to redirect output to (e.g. /dev/ttyACM0)
     * @param cmdStart Pointer to the start of the command
     * @param cmdLen Length of the command
     * @return true if successfully allocated and sent, false otherwise.
     */
    bool sendCommand(const char* targetPort, const char* cmdStart, size_t cmdLen);

private:
    KEYBOARD::KeyboardService* _keyboardService;
};

} // namespace USB_TERMINAL
