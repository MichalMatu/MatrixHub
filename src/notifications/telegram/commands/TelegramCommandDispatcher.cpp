/**
 * @file TelegramCommandDispatcher.cpp
 * @brief Implementation of command dispatcher
 */

#include "TelegramCommandDispatcher.h"
#include "CommandRegistry.h"
#include "TelegramReplyBuilder.h"
#include "../../../system/utils/Random.h"
#include "../../../system/logging/Logging.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "TgCmdDisp"

#include <string_view>
#include <algorithm>
#include <cctype>

namespace TELEGRAM::Commands {

namespace {

constexpr const char* kUnknownCommandJokes[] = {
    "I would answer that, but my firmware lawyer advised silence.",
    "That command is so secret even the ESP32 has never heard of it.",
    "I checked every stack frame. Still not a real command.",
    "Nice try. That spell is not in this firmware grimoire.",
    "I asked the matrix. It blinked in confusion.",
    "That command went straight to the bit bucket.",
    "Somewhere a parser is pretending this never happened.",
    "I searched flash, RAM, and vibes. Nothing.",
    "That command is not in the manual, the folklore, or the crash dump.",
    "Even the watchdog refused to acknowledge that command.",
    "I ran that through the decoder ring. Still nonsense.",
    "That command missed the release train.",
    "I asked the scheduler. It scheduled disappointment.",
    "This firmware supports many things. That is not one of them.",
    "I looked under /help. It was not there either.",
    "That command has been promoted to urban legend.",
    "I queried the heap. It could not allocate an answer.",
    "The command parser filed a polite complaint and moved on.",
    "That opcode belongs to an alternate universe build.",
    "I checked the changelog. No one admitted adding it.",
    "That command bounced off the silicon and into the void.",
    "I asked politely. The firmware still said no.",
    "Even autocorrect gave up on that one.",
    "That request was redirected to /dev/null for emotional support.",
};

constexpr size_t kUnknownCommandJokeCount =
    sizeof(kUnknownCommandJokes) / sizeof(kUnknownCommandJokes[0]);

void buildUnknownReply(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    const size_t jokeIndex = static_cast<size_t>(UTILS::RNG::rangeU32Exclusive(kUnknownCommandJokeCount));
    reply.line(kUnknownCommandJokes[jokeIndex]);
    reply.line();
    reply.line("/help");
    reply.finalize();
}

}  // namespace

bool TelegramCommandDispatcher::dispatch(const ParsedMessage& msg, CommandContext& ctx) {
    // Initialize context
    ctx.msg = &msg;
    ctx.response[0] = '\0';
    ctx.responseLen = 0;
    ctx.shouldReply = false;

    if (!msg.isCommand || msg.commandName.empty()) {
        if (msg.text[0] == '\0') {
            LOGD("Message is empty, ignoring");
            return false;
        }

        LOGI("Message is not a command, returning fallback reply");
        buildUnknownReply(ctx);
        return true;
    }

    LOGI("Dispatching command: /%.*s (args_len=%u, args redacted)",
         (int)msg.commandName.length(), msg.commandName.data(),
         (unsigned)msg.commandArgs.length());

    return findAndExecute(msg.commandName, ctx);
}

bool TelegramCommandDispatcher::findAndExecute(std::string_view commandName, CommandContext& ctx) {
    // Linear search through command registry with case-insensitive comparison
    for (size_t i = 0; i < kCommandCount; i++) {
        std::string_view regName(kCommands[i].name);

        if (commandName.length() == regName.length()) {
            bool match = std::equal(commandName.begin(), commandName.end(), regName.begin(),
                [](char a, char b) {
                    return std::tolower(static_cast<unsigned char>(a)) ==
                           std::tolower(static_cast<unsigned char>(b));
                });

            if (match) {
                LOGD("Found handler for /%.*s", (int)commandName.length(), commandName.data());

                bool result = kCommands[i].handler(ctx);

                LOGI("Command /%.*s executed: replied=%d len=%u",
                     (int)commandName.length(), commandName.data(),
                     ctx.shouldReply ? 1 : 0, (unsigned)ctx.responseLen);

                return result;
            }
        }
    }

    // Unknown command - send helpful response
    LOGI("Unknown command: /%.*s", (int)commandName.length(), commandName.data());

    buildUnknownReply(ctx);
    return true;  // We handled it (with error message)
}

}  // namespace TELEGRAM::Commands
