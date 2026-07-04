/**
 * @file PushoverTestSender.cpp
 * @brief Pushover test sender with synchronous delivery
 */

#include "PushoverTestSender.h"

#include "../../../notifications/runtime/PushoverTransportService.h"
#include "../../../notifications/settings/NotificationChannelStateView.h"
#include "../../../notifications/pushover/PushoverSendValidator.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/rtc/RtcConfig.h"

#undef LOG_TAG
#define LOG_TAG "PushoverTest"

namespace API {

PushoverTestSender::PushoverTestSender(NotificationSettingsService* settingsService,
                                       NOTIFICATIONS::PushoverTransportService* transport)
    : _settingsService(settingsService)
    , _transport(transport) {
    LOGI("PushoverTestSender Initialized");
}

bool PushoverTestSender::isConfigured() const {
    return NOTIFICATIONS::readPushoverChannelState(_settingsService).configured;
}

PushoverSendTestResult PushoverTestSender::sendTest(const char* message, size_t messageLen) {
    (void)messageLen;

    PushoverSendTestResult result = {};
    const auto channel = NOTIFICATIONS::readPushoverChannelState(_settingsService);

    NOTIFICATIONS::PUSHOVER::PushoverSendValidationInput input;
    input.settingsAvailable = channel.settingsAvailable;
    input.enabled = channel.enabled;
    input.configured = channel.configured;
    input.userKey = channel.userKey;
    input.apiToken = channel.apiToken;
    input.missingSettingsError = "not_initialized";

    const auto validation = NOTIFICATIONS::PUSHOVER::PushoverSendValidator::validate(input);
    if (!validation.ok) {
        result.error = validation.error;
        return result;
    }

    if (!_transport) {
        result.error = "transport_unavailable";
        return result;
    }

    // Same expectation as webhook tests: diagnostics should exercise the real
    // transport path, but they should not inherit the runtime cancel token.
    const auto dispatchResult = _transport->dispatchWithoutCancel(
        message,
        validation.userKey,
        validation.apiToken);
    result.success = dispatchResult.success;
    result.httpCode = dispatchResult.httpCode;
    result.error = dispatchResult.errorReason;

    if (result.success) {
        RTC::runtimeStats.pushoverSent++;
        RTC::runtimeStats.pushoverLastSendMs = millis();
        RTC::runtimeStats.pushoverLastHttpCode = static_cast<int16_t>(result.httpCode);
        LOGI("Test sent successfully (HTTP %d)", result.httpCode);
    } else {
        RTC::runtimeStats.pushoverFailed++;
        RTC::runtimeStats.pushoverLastSendMs = millis();
        RTC::runtimeStats.pushoverLastHttpCode = static_cast<int16_t>(result.httpCode);
        LOGW("Test failed: %s (HTTP %d)", result.error ? result.error : "unknown", result.httpCode);
    }

    return result;
}

}  // namespace API
