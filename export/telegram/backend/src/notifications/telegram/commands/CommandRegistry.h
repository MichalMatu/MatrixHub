/**
 * @file CommandRegistry.h
 * @brief Compile-time registry of available commands
 *
 * Uses constexpr array to avoid heap allocation for command registration.
 * Command names are stored in flash, not DRAM.
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

enum class HelpGroup {
    General,
    StatusDiagnostics,
    FeaturesDevices,
    Automation,
    ToolsSystem,
};

/**
 * Command handler function signature
 *
 * @param ctx Command context with message and response buffer
 * @return true if command was handled, false to try next handler
 */
using CommandHandler = bool (*)(CommandContext& ctx);

/**
 * Command definition
 */
struct CommandDef {
    const char* name;        // Command name without / (e.g. "status")
    const char* description; // Short description for /help
    HelpGroup group;         // Help section placement
    CommandHandler handler;
};

// Forward declarations of command handlers
bool handleHelp(CommandContext& ctx);
bool handleStatus(CommandContext& ctx);
bool handleSensors(CommandContext& ctx);
bool handleAlarms(CommandContext& ctx);
bool handleShelly(CommandContext& ctx);
bool handleTriggered(CommandContext& ctx);
bool handleBle(CommandContext& ctx);
bool handleScripts(CommandContext& ctx);
bool handleRun(CommandContext& ctx);
bool handleMacroStop(CommandContext& ctx);
bool handleIp(CommandContext& ctx);
bool handleMatrix(CommandContext& ctx);
bool handleReboot(CommandContext& ctx);
bool handleExec(CommandContext& ctx);
bool handleUsers(CommandContext& ctx);
bool handleHealth(CommandContext& ctx);

/**
 * Static command registry
 *
 * Add new commands here. The array is stored in flash.
 */
constexpr CommandDef kCommands[] = {
    {"help",       "Show available commands",               HelpGroup::General,           handleHelp},
    {"status",     "System status (uptime, heap, WiFi)",    HelpGroup::StatusDiagnostics, handleStatus},
    {"sensors",    "Current sensor readings",               HelpGroup::StatusDiagnostics, handleSensors},
    {"alarms",     "List alarm rules",                      HelpGroup::FeaturesDevices,   handleAlarms},
    {"triggered",  "Show active alarms",                    HelpGroup::FeaturesDevices,   handleTriggered},
    {"shelly",     "List/control Shelly devices",           HelpGroup::FeaturesDevices,   handleShelly},
    {"ble",        "BLE thermometer readings",              HelpGroup::FeaturesDevices,   handleBle},
    {"scripts",    "List macro scripts",                    HelpGroup::Automation,        handleScripts},
    {"run",        "Run macro script",                      HelpGroup::Automation,        handleRun},
    {"macro_stop", "Stop running macro",                    HelpGroup::Automation,        handleMacroStop},
    {"ip",         "Check external IP address",             HelpGroup::StatusDiagnostics, handleIp},
    {"matrix",     "Send text to matrix display",           HelpGroup::FeaturesDevices,   handleMatrix},
    {"reboot",     "Restart device",                        HelpGroup::ToolsSystem,       handleReboot},
    {"exec",       "Execute shell commands via USB",        HelpGroup::ToolsSystem,       handleExec},
    {"users",      "List system users",                     HelpGroup::ToolsSystem,       handleUsers},
    {"health",     "Deep system diagnostics and telemetry", HelpGroup::StatusDiagnostics, handleHealth},
};

constexpr size_t kCommandCount = sizeof(kCommands) / sizeof(kCommands[0]);

}  // namespace TELEGRAM::Commands
