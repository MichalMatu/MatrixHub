/**
 * @file TelegramCommandParser.h
 * @brief Parser for Telegram update messages with command extraction
 *
 * Extends the existing TelegramUpdateParser to extract message text
 * and parse commands (messages starting with /).
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

class TelegramCommandParser {
public:
    /**
     * Parse command from message text
     *
     * Extracts command name and args from text like "/status arg1 arg2"
     * Sets isCommand=true and fills commandName and commandArgs.
     *
     * @param msg ParsedMessage with text already filled
     */
    static void parseCommand(ParsedMessage& msg);
};

}  // namespace TELEGRAM::Commands
