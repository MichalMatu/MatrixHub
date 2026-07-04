/**
 * @file BleCommand.cpp
 * @brief /ble command - display BLE thermometer readings from RTC cache
 */

#include "BleCommand.h"
#include "TelegramCommandSections.h"
#include "TelegramReplyBuilder.h"

namespace TELEGRAM::Commands {

bool handleBle(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    Sections::appendBleSection(reply, true, true);
    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
