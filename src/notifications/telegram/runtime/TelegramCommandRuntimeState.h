/**
 * @file TelegramCommandRuntimeState.h
 * @brief Shared PSRAM-backed state for Telegram command polling/dispatch
 */

#pragma once

#include "../commands/TelegramCommandTypes.h"

namespace TELEGRAM {

struct TelegramCommandRuntimeState {
    Commands::ParsedMessage parsedMessage = {};
    Commands::CommandContext commandContext = {};
};

}  // namespace TELEGRAM
