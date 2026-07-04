#include "PowerSleepController.h"
#include "PowerWakeController.h"
#include "../logging/Logging.h"
#include "../rtc/RtcConfig.h"
#include "../../config/App.h"
#include "../../config/TaskConfig.h"

#ifndef NATIVE_BUILD
#include <services/SleepService.h>
#endif

#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace POWER {

void PowerSleepController::begin(PowerWakeController* wakeController) {
    _wakeController = wakeController;
    _sleepRequested = false;
    _sleepRequestAtMs = 0;
    _sleepDelayMs = 0;
    _isEnteringDeepSleep = false;
    _deepSleepStarted = false;
    _wakeConfig = WakeSourceConfig{
        POWER::WAKE_INTERVAL_MS,
        POWER::TOUCH_WAKE_GPIO,
        POWER::TOUCH_WAKE_THRESHOLD,
        true,
        true,
        false
    };
}

void PowerSleepController::setWakeInterval(uint32_t intervalMs) {
    _wakeConfig.wakeIntervalMs = intervalMs;
    _wakeConfig.timerEnabled = true;
}

void PowerSleepController::setWakeConfig(const WakeSourceConfig& config) {
    _wakeConfig = config;
}

uint32_t PowerSleepController::getWakeInterval() {
    return _wakeConfig.wakeIntervalMs;
}

void PowerSleepController::startSleepFailsafe() {
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        sleepFailsafeTask,
        "SleepFailsafe",
        CONFIG::TASKS::STACK_SMALL,
        this,
        CONFIG::TASKS::PRIO_WIFI_RECOVERY,
        &taskHandle,
        CONFIG::TASKS::CORE_PRO);

    if (created != pdPASS) {
        LOGW("[Power] Failed to start sleep failsafe task");
    }
}

void PowerSleepController::sleepFailsafeTask(void* arg) {
    auto* self = static_cast<PowerSleepController*>(arg);
    vTaskDelay(pdMS_TO_TICKS(POWER::SLEEP_ENTRY_FAILSAFE_MS));

    if (!self || !self->_isEnteringDeepSleep || self->_deepSleepStarted) {
        vTaskDelete(nullptr);
        return;
    }

    LOGE("[Power] Sleep entry failsafe fired after %lus; forcing deep sleep",
         static_cast<unsigned long>(POWER::SLEEP_ENTRY_FAILSAFE_MS / 1000UL));

    if (!self->_wakeController || !self->_wakeController->configureWakeSources(self->_wakeConfig)) {
        LOGE("[Power] Sleep failsafe could not arm a wake source; cancelling forced deep sleep");
        self->_isEnteringDeepSleep = false;
        self->_sleepRequested = false;
        vTaskDelete(nullptr);
        return;
    }

#ifdef ESP_PD_DOMAIN_RTC_SLOW_MEM
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#endif
#ifdef ESP_PD_DOMAIN_RTC_FAST_MEM
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#endif

    RTC::prepareForSleep();
    self->_deepSleepStarted = true;
    vTaskDelay(pdMS_TO_TICKS(50));
    esp_deep_sleep_start();
    vTaskDelete(nullptr);
}

void PowerSleepController::requestSleep(const char *reason, uint32_t delayMs) {
    // Force minimum delay to avoid executing shutdown synchronously
    // inside caller's context (e.g. HTTP handler → MDNS.end() deadlock).
    static constexpr uint32_t MIN_SLEEP_DELAY_MS = 50;
    if (delayMs < MIN_SLEEP_DELAY_MS) {
        delayMs = MIN_SLEEP_DELAY_MS;
    }

    _sleepRequested = true;
    _sleepDelayMs = delayMs;
    _sleepRequestAtMs = millis();
    _sleepReason = reason;
    LOGI("[Power] Sleep requested (%s, +%lu ms)", reason ? reason : "unknown", static_cast<unsigned long>(delayMs));
}

bool PowerSleepController::isSleepRequested() {
    return _sleepRequested;
}

uint32_t PowerSleepController::getSleepEtaMs() {
    if (!_sleepRequested) {
        return 0;
    }
    uint32_t now = millis();
    uint32_t elapsed = now - _sleepRequestAtMs;
    if (elapsed >= _sleepDelayMs) {
        return 0;
    }
    return _sleepDelayMs - elapsed;
}

void PowerSleepController::loopTick() {
    if (_sleepRequested) {
        uint32_t now = millis();
        if (now - _sleepRequestAtMs >= _sleepDelayMs) {
            enterDeepSleep(_sleepReason ? _sleepReason : "pending");
        }
    }
}

void PowerSleepController::enterDeepSleep(const char *reason) {
    if (_sleepCallback) {
        _sleepCallback(reason);
        return;
    }

    if (_isEnteringDeepSleep) {
        LOGW("[Power] enterDeepSleep() called twice; ignoring (reason=%s)", reason ? reason : "unknown");
        return;
    }
    _isEnteringDeepSleep = true;
    _deepSleepStarted = false;
    _sleepRequested = true;
    _sleepDelayMs = 0;
    startSleepFailsafe();

    LOGI("[Power] Entering deep sleep (reason: %s). Wake timer: %s %.1fs, button:%s, touch:%s GPIO%u.",
         reason ? reason : "unknown",
         _wakeConfig.timerEnabled ? "ON" : "OFF",
         static_cast<float>(_wakeConfig.wakeIntervalMs) / 1000.0f,
         _wakeConfig.buttonEnabled ? "ON" : "OFF",
         _wakeConfig.touchEnabled ? "ON" : "OFF",
         static_cast<unsigned>(_wakeConfig.touchGpio));

    // Call framework sleep callbacks
    SleepService::executeSleepCallbacks();
    LOGI("[Power] Sleep callbacks executed");

    // Call application pre-sleep hook
    if (_preSleepHook) {
        _preSleepHook();
    }

    if (!_wakeController || !_wakeController->configureWakeSources(_wakeConfig)) {
        LOGE("[Power] Wake source configuration failed; cancelling deep sleep");
        _isEnteringDeepSleep = false;
        _sleepRequested = false;
        return;
    }

#ifdef ESP_PD_DOMAIN_RTC_SLOW_MEM
    // Explicitly keep RTC Slow Memory powered (where RTC_DATA_ATTR lives)
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_ON);
#endif
#ifdef ESP_PD_DOMAIN_RTC_FAST_MEM
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_ON);
#endif

    // Final RTC snapshot just before sleep entry (after pre-sleep hook and task stop).
    RTC::prepareForSleep();

    LOGI("[Power] Calling esp_deep_sleep_start() now");
    _deepSleepStarted = true;
    vTaskDelay(pdMS_TO_TICKS(100)); // Give UART and FS a moment to flush (RTOS-safe)
    esp_deep_sleep_start();
}

const char* PowerSleepController::getSleepReason() {
    return _sleepReason;
}

void PowerSleepController::setPreSleepHook(void (*hook)()) {
    _preSleepHook = hook;
}

void PowerSleepController::setSleepCallback(void (*callback)(const char *)) {
    _sleepCallback = callback;
}

void PowerSleepController::resetTestHooks() {
    _sleepCallback = nullptr;
}

} // namespace POWER
