/**
 * @file RebootCommand.h
 * @brief Telegram command to restart the device
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

bool handleReboot(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
