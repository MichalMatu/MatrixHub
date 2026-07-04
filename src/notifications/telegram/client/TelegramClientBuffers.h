/**
 * @file TelegramClientBuffers.h
 * @brief PSRAM-backed scratch buffers owned by TelegramClient
 */

#pragma once

#include "../../../config/App.h"

namespace TELEGRAM {

/**
 * Shared scratch buffers for Telegram URL/payload construction.
 *
 * These used to live as file-static EXT_RAM_BSS_ATTR globals. They are now
 * explicitly owned by TelegramClient and passed to helper modules, so the whole
 * Telegram transport path keeps the same zero-heap behavior without hidden
 * process-wide state.
 */
struct TelegramClientBuffers {
    char url[APP::NOTIFICATIONS::TELEGRAM_BUFFER_SIZE_URL]{};
    char payload[APP::NOTIFICATIONS::TELEGRAM_BUFFER_SIZE_PAYLOAD]{};
};

}  // namespace TELEGRAM
