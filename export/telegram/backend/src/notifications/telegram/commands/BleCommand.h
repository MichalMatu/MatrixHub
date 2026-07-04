/**
 * @file BleCommand.h
 * @brief /ble command - display BLE thermometer readings
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /ble command
 * Shows BLE thermometer readings from RTC cache
 */
bool handleBle(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
