#include "WebhookNotifier.h"
#include "WebhookWorker.h"
#include "WebhookSendValidator.h"
#include "JsonEscaper.h"
#include "../settings/NotificationChannelStateView.h"
#include <esp_heap_caps.h>
#include <system/logging/Logging.h>

#undef LOG_TAG
#define LOG_TAG "WebHookNotify"

namespace NOTIFICATIONS {
namespace {

bool isDiscordWebhookUrl(const char* url) {
    if (!url) return false;
    return strstr(url, "discord.com/api/webhooks") != nullptr ||
           strstr(url, "discordapp.com/api/webhooks") != nullptr;
}

const char* selectPayloadKey(const char* url) {
    return isDiscordWebhookUrl(url) ? "content" : "text";
}
}  // namespace

WebhookNotifier::WebhookNotifier(NotificationSettingsService* settings, WebhookWorker* worker)
    : _settings(settings), _worker(worker) {
    // Recheck note:
    // AlarmNotificationBridge is the main production caller of sendTextMessage().
    // Give that path one reusable PSRAM JSON buffer so repeated alarms avoid a
    // malloc/free pair per notification. The notifier still has a fallback
    // path below, so boot does not fail just because this optimization buffer
    // could not be reserved.
    _textScratch = SYSTEM::MEMORY::makeUniqueInPsram<TextPayloadScratch>();
    if (!_textScratch) {
        LOGW("Webhook text scratch unavailable in PSRAM; falling back to per-send allocation");
    }
}

bool WebhookNotifier::isConfigured() const {
    return readWebhookChannelState(_settings).configured;
}

bool WebhookNotifier::isEnabled() const {
    return readWebhookChannelState(_settings).enabled;
}

WebhookSendResult WebhookNotifier::sendMessage(const char* payload, size_t len) {
    WebhookSendResult result;
    const auto channel = readWebhookChannelState(_settings);

    WEBHOOK::WebhookSendValidationInput input;
    input.settingsAvailable = channel.settingsAvailable;
    input.enabled = channel.enabled;
    input.configured = channel.configured;
    input.url = channel.url;
    input.payloadLen = len;

    const auto validation = WEBHOOK::WebhookSendValidator::validate(input);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }

    if (_worker && _worker->enqueue(payload, len)) {
        result.queued = true;
    } else {
        result.error = _worker ? "queue_full" : "worker_not_set";
    }
    
    return result;
}

WebhookSendResult WebhookNotifier::sendTextMessage(const char* message, size_t len) {
    WebhookSendResult result;
    const auto channel = readWebhookChannelState(_settings);

    WEBHOOK::WebhookSendValidationInput input;
    input.settingsAvailable = channel.settingsAvailable;
    input.enabled = channel.enabled;
    input.configured = channel.configured;
    input.url = channel.url;
    input.payloadLen = len;

    const auto validation = WEBHOOK::WebhookSendValidator::validate(input);
    if (!validation.ok || !message) {
        result.error = validation.ok ? "invalid_length" : validation.error;
        return result;
    }

    const char* key = selectPayloadKey(validation.url);

    char* json = _textScratch ? _textScratch->json : nullptr;
    bool usingFallbackBuffer = false;
    if (!json) {
        // Fallback preserves the old behavior for low-memory boots or tests
        // where the reusable scratch was not available. If churn ever reappears
        // in production logs, first check whether this branch is being hit.
        json = static_cast<char*>(heap_caps_malloc(
            CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        if (!json) {
            LOGE("OOM (PSRAM): failed to allocate %u bytes for webhook JSON",
                 CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD);
            result.error = "oom";
            return result;
        }
        usingFallbackBuffer = true;
    }

    size_t jsonLen = JsonEscaper::wrapAsKeyJson(key, message, len, json, CONFIG::NOTIFICATIONS::WEBHOOK::MAX_PAYLOAD);
    if (jsonLen == 0) {
        result.error = "payload_format_error";
        if (usingFallbackBuffer) {
            heap_caps_free(json);
        }
        return result;
    }

    // Normal production path:
    // - wrap plain text into JSON using the reusable buffer,
    // - enqueue it into WebhookWorker's fixed PSRAM queue,
    // - return without any additional per-message heap ownership.
    result = sendMessage(json, jsonLen);
    if (usingFallbackBuffer) {
        heap_caps_free(json);
    }
    return result;
}

}  // namespace NOTIFICATIONS
