/**
 * @file TelegramPoller.h
 * @brief Telegram command polling and dispatch handler
 * 
 * Extracted from TelegramWorker.cpp (374 LOC → modular architecture)
 * Handles long-polling for incoming commands and dispatching them.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../runtime/TelegramCommandRuntimeState.h"
#include "../runtime/TelegramRuntimeConfigView.h"

namespace TELEGRAM {

struct WorkerStatus;
class MessageQueue;

/**
 * @brief Polls for incoming Telegram commands and dispatches them
 * 
 * Responsibilities:
 * - Execute long-poll requests for updates
 * - Parse and authorize incoming messages
 * - Dispatch commands and queue responses
 * - Update status counters and track last update ID
 */
class TelegramClient;

class TelegramPoller {
public:
    /**
     * @brief Poll for commands and dispatch them
     * @param lastUpdateId Reference to last update ID (will be updated)
     * @param status Reference to status structure
     * @param config Runtime Telegram configuration snapshot
     */
    static void pollAndDispatch(int64_t& lastUpdateId, 
                               WorkerStatus& status,
                               const TelegramRuntimeConfigView& config,
                               TelegramClient* client,
                               SemaphoreHandle_t statusMutex,
                               MessageQueue* outboundQueue,
                               TelegramCommandRuntimeState& runtimeState,
                               char* discoveredChatId,
                               size_t discoveredChatIdCapacity);

private:
    /**
     * @brief Check if chat ID is authorized
     * @param chatId Chat ID to check
     * @param authorizedChatId Authorized chat ID snapshot
     * @return true if authorized
     */
    static bool isAuthorized(const char* chatId, const char* authorizedChatId);
};

}  // namespace TELEGRAM
