/**
 * @file ShellyCommand.h
 * @brief /shelly command handler declaration
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /shelly command - list configured Shelly devices
 */
bool handleShelly(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
