/**
 * @file TelegramCommandTypes.h
 * @brief Core data types for Telegram Commands module
 *
 * All structures use fixed-size char arrays to avoid heap fragmentation.
 * No String usage in hot paths.
 */

#pragma once

#include <string_view>

namespace ALARMS { class AlarmService; }
namespace MACROS { class MacroService; }
namespace MATRIX_MANAGER { class MatrixManagerService; }
namespace USB_TERMINAL { class UsbTerminalService; }
namespace SHELLY { class ShellyService; }
class ServiceRegistry;
class SecurityManager;

namespace TELEGRAM::Commands {

// Buffer size constants
constexpr size_t CMD_TEXT_MAX = 256;        // Max incoming command text
constexpr size_t CMD_RESPONSE_MAX = 1024;   // Max response text (matches Telegram outbound queue limit)
constexpr size_t CMD_CHAT_ID_MAX = 24;      // Chat ID buffer (matches Telegram outbound queue limit)
constexpr size_t CMD_USERNAME_MAX = 33;     // Telegram username max + null
constexpr size_t CMD_NAME_MAX = 32;         // Command name without /

/**
 * Parsed message from Telegram update
 */
struct ParsedMessage {
    int64_t updateId;
    char chatId[CMD_CHAT_ID_MAX];
    char text[CMD_TEXT_MAX];
    char fromUsername[CMD_USERNAME_MAX];
    int64_t date;

    // Parsed command parts (if message starts with /)
    bool isCommand;
    std::string_view commandName;      // Points into text[] after command
    std::string_view commandArgs;      // Points into text[] after command name
};

/**
 * Command execution context
 */
struct CommandContext {
    const ParsedMessage* msg;
    char response[CMD_RESPONSE_MAX];
    size_t responseLen;
    bool shouldReply;                  // Set to true if handler wants to send response
    ALARMS::AlarmService* alarmService; // [Dependency Injection]
    MACROS::MacroService* macroService;  // [Dependency Injection]
    MATRIX_MANAGER::MatrixManagerService* matrixManager; // [Dependency Injection]
    SecurityManager* securityManager; // [Dependency Injection]
    USB_TERMINAL::UsbTerminalService* usbTerminalService; // [Dependency Injection]
    SHELLY::ShellyService* shellyService; // [Dependency Injection]
    // Intentional compatibility escape hatch for a few system-level commands
    // that still need broad runtime coordination (for example reboot/reset
    // flows). Leave this field for now; narrowing it further only becomes worth
    // it if the Telegram command surface grows and more handlers start touching
    // ServiceRegistry directly.
    ServiceRegistry* registry; // [Dependency Injection]
};

}  // namespace TELEGRAM::Commands
