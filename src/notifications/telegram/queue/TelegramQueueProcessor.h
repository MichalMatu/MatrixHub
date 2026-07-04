/**
 * @file TelegramQueueProcessor.h
 * @brief Telegram outbound message queue processor
 * 
 * Extracted from TelegramWorker.cpp (374 LOC → modular architecture)
 * Handles processing of queued outbound messages with rate limiting.
 */

#pragma once

#include <cstdint>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace TELEGRAM {

struct WorkerStatus;
struct TelegramRuntimeConfigView;
class MessageQueue;

/**
 * @brief Processes queued outbound Telegram messages
 * 
 * Responsibilities:
 * - Dequeue and send one outbound message per worker turn
 * - Handle network mutex coordination
 * - Update status counters
 */
class TelegramClient;

struct OutboundProcessResult {
    bool didWork = false;
    bool applyQueueDelay = false;
};

class TelegramQueueProcessor {
public:
    /**
     * @brief Process at most one queued outbound message
     * @param status Reference to status structure (for counters)
     */
    static OutboundProcessResult processOneQueuedMessage(WorkerStatus& status,
                                                         TelegramClient* client,
                                                         const TelegramRuntimeConfigView& config,
                                                         SemaphoreHandle_t statusMutex,
                                                         MessageQueue* queue);
};

}  // namespace TELEGRAM
