/**
 * @file HelpCommand.h
 * @brief /help command handler
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /help command
 * Lists all available commands with descriptions
 */
bool handleHelp(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
