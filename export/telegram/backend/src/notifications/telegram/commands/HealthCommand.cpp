/**
 * @file HealthCommand.cpp
 * @brief Implementation of /health command
 */

#include "HealthCommand.h"
#include "TelegramCommandSections.h"
#include "TelegramReplyBuilder.h"

namespace TELEGRAM::Commands {

bool handleHealth(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    Sections::appendHealthSection(reply, true, true);
    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
