#pragma once

#include <cstdint>
#include <cstddef>

#include "../commands/TelegramCommandTypes.h"

namespace TELEGRAM {

struct UpdateLite {
    bool hasMessage = false;
    int64_t updateId = 0;
    char chatId[Commands::CMD_CHAT_ID_MAX] = {0};
    char text[Commands::CMD_TEXT_MAX] = {0};
    char fromUsername[Commands::CMD_USERNAME_MAX] = {0};
    int64_t date = 0;
};

using UpdateLiteCallback = bool (*)(const UpdateLite& update, void* user);

}  // namespace TELEGRAM
