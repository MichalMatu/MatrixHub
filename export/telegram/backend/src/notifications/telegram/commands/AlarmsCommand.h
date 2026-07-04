/**
 * @file AlarmsCommand.h
 * @brief /alarms command handler declaration
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /alarms command - list configured alarm rules
 */
bool handleAlarms(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
