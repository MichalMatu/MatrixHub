/**
 * @file AlarmNotifier.h
 * @brief Alarm notification dispatcher (facade)
 * 
 * Routes alarm notifications to injected notification backends:
 * - Telegram
 * - Webhook
 * - Pushover
 * 
 * Uses fixed-size buffers - no heap allocation.
 * Message building delegated to AlarmMessageBuilder.
 */

#pragma once

#include <functional>
#include "../types/AlarmTypes.h"
#include "../engine/AlarmEvaluator.h"

namespace ALARMS {

/** Maximum message length for notifications */
constexpr size_t kMaxNotifyMsgLen = 256;   // Stack-allocated in notify() – keep small! Actual alarm msgs are ~100 bytes

struct AlarmNotificationBackend {
    std::function<bool(const char*, size_t)> telegramSend;
    std::function<bool(const char*, size_t)> webhookSend;
    std::function<bool(const char*, size_t)> pushoverSend;
};

/**
 * Result of notification attempt
 */
struct NotifyResult {
    bool telegramQueued;
    bool webhookSent;
    bool pushoverQueued;
    const char* error;
    
    NotifyResult() 
        : telegramQueued(false), webhookSent(false), pushoverQueued(false),
          error(nullptr) {}
    
    bool anySuccess() const {
        return telegramQueued || webhookSent || pushoverQueued;
    }
};

/**
 * Alarm notification dispatcher (facade)
 * 
 * Coordinates notification routing through injected notification backends.
 * 
 * Usage:
 *   EvaluationResult eval = AlarmEvaluator::evaluate(...);
 *   if (eval.shouldNotify) {
 *       NotifyResult result = notifier.notify(eval);
 *   }
 */
class AlarmNotifier {
public:
    void setBackend(AlarmNotificationBackend backend) { _backend = std::move(backend); }

    /**
     * Send notification for a triggered alarm
     * 
     * @param eval Evaluation result from AlarmEvaluator
     * @return Result indicating which channels were used
     */
    NotifyResult notify(const EvaluationResult& eval) const;
    
    /**
     * Send notification for alarm cleared (optional)
     * 
     * @param eval Evaluation result (with triggered=false, stateChanged=true)
     * @return Result
     */
    NotifyResult notifyCleared(const EvaluationResult& eval) const;

private:
    AlarmNotificationBackend _backend;
};

}  // namespace ALARMS
