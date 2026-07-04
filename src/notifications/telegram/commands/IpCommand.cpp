/**
 * @file IpCommand.cpp
 * @brief Implementation of /ip command
 */

#include "IpCommand.h"
#include "TelegramCommandSections.h"
#include "TelegramReplyBuilder.h"

namespace TELEGRAM::Commands {

bool handleIp(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    Sections::appendExternalIpSection(reply, Sections::fetchExternalIp(), true);
    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
