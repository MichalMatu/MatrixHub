/**
 * @file TriggeredCommand.h
 * @brief /triggered command handler declaration
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /triggered command - show currently active alarms
 */
bool handleTriggered(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
