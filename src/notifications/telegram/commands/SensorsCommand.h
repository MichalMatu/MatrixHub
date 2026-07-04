/**
 * @file SensorsCommand.h
 * @brief /sensors command handler
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /sensors command
 * Shows current sensor readings
 */
bool handleSensors(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
