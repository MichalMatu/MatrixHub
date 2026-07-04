#pragma once

#include "TelegramCommandTypes.h"

#include <string_view>

namespace TELEGRAM::Commands {

class TelegramCommandDispatcher {
public:
    /**
     * Dispatch a parsed message to the appropriate command handler
     *
     * @param msg Parsed message from Telegram
     * @param ctx Command context to fill with response
     * @return true if a command was handled, false if message was not a command
     *         or no handler matched
     */
    static bool dispatch(const ParsedMessage& msg, CommandContext& ctx);

private:
    /**
     * Find and execute handler for a command
     *
     * @param commandName Command name without /
     * @param ctx Command context
     * @return true if handler was found and executed
     */
    static bool findAndExecute(std::string_view commandName, CommandContext& ctx);
};

}  // namespace TELEGRAM::Commands
