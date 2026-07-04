/**
 * @file AlarmNotifier.cpp
 * @brief Implementation of alarm notification dispatcher (facade)
 */

#include "AlarmNotifier.h"
#include "AlarmMessageBuilder.h"
#include "../../system/rtc/RtcConfig.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "AlarmNotify"

namespace ALARMS {

NotifyResult AlarmNotifier::notify(const EvaluationResult& eval) const {
    NotifyResult result;
    
    if (!eval.rule || !eval.triggered) {
        result.error = "Invalid evaluation result";
        return result;
    }

    // FIX: Increment global alarm trigger counter
    RTC::runtimeStats.alarmsTriggered++;
    
    const AlarmRule& rule = *eval.rule;
    
    // Build notification message
    char msgBuffer[kMaxNotifyMsgLen];
    size_t msgLen = AlarmMessageBuilder::build(eval, msgBuffer, sizeof(msgBuffer), false);
    
    // Send via configured channels
    // Note: LED is handled by AlarmCoordinator (latching logic) - not here.
    // This notifier only handles edge-triggered notifications with cooldown.
    if (hasChannel(rule.notifyChannels, NotifyChannel::Telegram) && _backend.telegramSend) {
        result.telegramQueued = _backend.telegramSend(msgBuffer, msgLen);
    }
    
    if (hasChannel(rule.notifyChannels, NotifyChannel::Webhook) && _backend.webhookSend) {
        result.webhookSent = _backend.webhookSend(msgBuffer, msgLen);
    }

    if (hasChannel(rule.notifyChannels, NotifyChannel::Pushover) && _backend.pushoverSend) {
        result.pushoverQueued = _backend.pushoverSend(msgBuffer, msgLen);
    }
    
    // Shelly control is now handled exclusively by AlarmCoordinator
    // to avoid double-actuation and separate concerns.
    // if (rule.hasShellyDevices()) { ... }
    
    return result;
}

NotifyResult AlarmNotifier::notifyCleared(const EvaluationResult& eval) const {
    NotifyResult result;
    
    if (!eval.rule) {
        result.error = "Invalid evaluation result";
        return result;
    }
    
    const AlarmRule& rule = *eval.rule;
    
    // Build "cleared" message
    char msgBuffer[kMaxNotifyMsgLen];
    size_t msgLen = AlarmMessageBuilder::build(eval, msgBuffer, sizeof(msgBuffer), true);
    
    // Only send Telegram for cleared notifications (LED already off)
    if (hasChannel(rule.notifyChannels, NotifyChannel::Telegram) && _backend.telegramSend) {
        result.telegramQueued = _backend.telegramSend(msgBuffer, msgLen);
    }

    if (hasChannel(rule.notifyChannels, NotifyChannel::Webhook) && _backend.webhookSend) {
        result.webhookSent = _backend.webhookSend(msgBuffer, msgLen);
    }

    if (hasChannel(rule.notifyChannels, NotifyChannel::Pushover) && _backend.pushoverSend) {
        result.pushoverQueued = _backend.pushoverSend(msgBuffer, msgLen);
    }
    
    return result;
}

}  // namespace ALARMS
