/**
 * @file MacroCommands.h
 * @brief Telegram command handlers for macro scripts
 *
 * Commands:
 *   /scripts  - List available macro scripts
 *   /run      - Run a macro script by name
 *   /macro_stop - Stop the currently running macro
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

bool handleScripts(CommandContext& ctx);
bool handleRun(CommandContext& ctx);
bool handleMacroStop(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
