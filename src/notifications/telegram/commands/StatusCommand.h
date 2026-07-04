/**
 * @file StatusCommand.h
 * @brief /status command handler
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /status command
 * Shows system status: uptime, free heap, WiFi info
 */
bool handleStatus(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
