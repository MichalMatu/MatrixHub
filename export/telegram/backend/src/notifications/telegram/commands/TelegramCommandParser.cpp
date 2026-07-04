/**
 * @file TelegramCommandParser.cpp
 * @brief Implementation of Telegram command parser
 */

#include "TelegramCommandParser.h"

#include <cstring>

#include <string_view>

namespace TELEGRAM::Commands {

void TelegramCommandParser::parseCommand(ParsedMessage& msg) {
    msg.isCommand = false;
    msg.commandName = std::string_view();
    msg.commandArgs = std::string_view();

    if (msg.text[0] != '/') {
        return;
    }

    // Find end of command name (space, @, or end of string)
    const char* textPtr = msg.text + 1;  // Skip /
    size_t cmdLen = 0;

    while (textPtr[cmdLen] != '\0' && textPtr[cmdLen] != ' ' && textPtr[cmdLen] != '@') {
        cmdLen++;
    }

    if (cmdLen == 0) {
        return;  // Just "/" with nothing after
    }

    // Point commandName to the original text fragment
    msg.commandName = std::string_view(textPtr, cmdLen);

    // Find args (skip @botname if present, then skip spaces)
    const char* argsStart = textPtr + cmdLen;

    // Skip @botname
    if (*argsStart == '@') {
        while (*argsStart != '\0' && *argsStart != ' ') {
            argsStart++;
        }
    }

    // Skip leading spaces
    while (*argsStart == ' ') {
        argsStart++;
    }

    if (*argsStart != '\0') {
        msg.commandArgs = std::string_view(argsStart);
    }

    msg.isCommand = true;
}

}  // namespace TELEGRAM::Commands
