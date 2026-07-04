#include "PowerWakeController.h"
#include "../logging/Logging.h"
#include "../../config/App.h"
#include "../../config/Hardware.h"
#include <esp_idf_version.h>
#include <soc/soc_caps.h>  // For SOC_PM_SUPPORT_EXT1_WAKEUP

extern "C" {
    // Not all targets / Arduino-ESP32 variants expose this symbol.
    // Declare it weak so the code compiles everywhere and we can detect support at runtime.
    uint64_t esp_sleep_get_gpio_wakeup_status(void) __attribute__((weak));
}

namespace POWER {


namespace {
bool isTouchWakeGpioAllowed(uint8_t gpio) {
    return gpio >= POWER::TOUCH_WAKE_GPIO_MIN && gpio <= POWER::TOUCH_WAKE_GPIO_MAX;
}

bool armTimerWake(uint32_t intervalMs) {
    const esp_err_t err = esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(intervalMs) * 1000ULL);
    if (err != ESP_OK) {
        LOGE("[Power] Timer wake failed for %lums (%d)",
             static_cast<unsigned long>(intervalMs),
             err);
        return false;
    }
    return true;
}
}  // namespace


void PowerWakeController::begin() {
    _wakeupCause = esp_sleep_get_wakeup_cause();
    _gpioWakeupMask = 0;
    _ext1WakeupMask = 0;

    // Capture wake pin masks when available (ESP-IDF specifics).
    // For ESP_SLEEP_WAKEUP_GPIO (only when supported by the linked IDF/libs).
    if (esp_sleep_get_gpio_wakeup_status) {
        _gpioWakeupMask = esp_sleep_get_gpio_wakeup_status();
    }
    // For ESP_SLEEP_WAKEUP_EXT1 (bitmask of RTC IOs used in ext1).
    // Note: ESP32-S3 supports EXT1 wakeup.
#if SOC_PM_SUPPORT_EXT1_WAKEUP
    _ext1WakeupMask = esp_sleep_get_ext1_wakeup_status();
#endif

    switch (_wakeupCause) {
        case ESP_SLEEP_WAKEUP_TIMER:
            _wakeReason = WakeReason::Timer;
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            _wakeReason = WakeReason::Touch;
            break;
#if defined(ESP_SLEEP_WAKEUP_GPIO)
        case ESP_SLEEP_WAKEUP_GPIO:
            _wakeReason = WakeReason::Button;
            break;
#endif
#if SOC_PM_SUPPORT_EXT1_WAKEUP || SOC_PM_SUPPORT_EXT0_WAKEUP
        case ESP_SLEEP_WAKEUP_EXT0:
        case ESP_SLEEP_WAKEUP_EXT1:
            _wakeReason = WakeReason::Button;
            break;
#endif
        case ESP_SLEEP_WAKEUP_UNDEFINED:
            _wakeReason = WakeReason::Unknown;
            break;
        default:
            _wakeReason = WakeReason::Other;
            break;
    }
}

WakeReason PowerWakeController::getWakeReason() {
    return _wakeReason;
}

esp_sleep_wakeup_cause_t PowerWakeController::getWakeupCauseRaw() {
    return _wakeupCause;
}

uint64_t PowerWakeController::getGpioWakeupMask() {
    return _gpioWakeupMask;
}

uint64_t PowerWakeController::getExt1WakeupMask() {
    return _ext1WakeupMask;
}

bool PowerWakeController::configureWakeSources(const WakeSourceConfig& config) {
    if (_configureWakeSourcesCallback) {
        _configureWakeSourcesCallback();
        return true;
    }

    bool timerArmed = false;
    bool buttonArmed = false;
    bool touchArmed = false;

    // Configure Wake Sources (ESP32-S3 Optimized)
    // ESP32-S3 uses EXT1 wakeup for RTC GPIOs (e.g. GPIO0 BOOT button) from Deep Sleep.

    if (config.timerEnabled && config.wakeIntervalMs > 0) {
        timerArmed = armTimerWake(config.wakeIntervalMs);
    }

    if (config.buttonEnabled && HW::USER_BUTTON != HW::PIN_DISABLED) {
        // BOOT/USER button wakeup configuration using EXT1
        // EXT1 supports waking up when specific RTC GPIOs go to specified level.
        const uint64_t wakeGpioMask = (1ULL << HW::USER_BUTTON);
        const auto wakeMode = HW::USER_BUTTON_ACTIVE_LOW
            ? ESP_EXT1_WAKEUP_ANY_LOW
            : ESP_EXT1_WAKEUP_ANY_HIGH;

        // Configure GPIO as input with pull-up to ensure stable state
        // Note: On S3, we might need rtc_gpio_pullup_en() but gpio_config with RTC mapping usually handles it.
        gpio_config_t io_conf = {};
        io_conf.pin_bit_mask = wakeGpioMask;
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = HW::USER_BUTTON_ACTIVE_LOW ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io_conf.pull_down_en = HW::USER_BUTTON_ACTIVE_LOW ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
        io_conf.intr_type = GPIO_INTR_DISABLE;
        gpio_config(&io_conf);

        // Enable EXT1 Wakeup (Supported on S3 for Deep Sleep)
        // Wake up when the physical button reaches its asserted level.
        esp_err_t err = esp_sleep_enable_ext1_wakeup(wakeGpioMask, wakeMode);
        if (err == ESP_OK) {
            buttonArmed = true;
        } else {
            LOGW("[Power] EXT1 wake failed for GPIO%u (%d)",
                 static_cast<unsigned>(HW::USER_BUTTON),
                 err);
        }
    } else if (config.buttonEnabled) {
        LOGW("[Power] Wake button requested but board pin is disabled");
    }

#if SOC_TOUCH_SENSOR_SUPPORTED
    if (config.touchEnabled) {
        if (isTouchWakeGpioAllowed(config.touchGpio)) {
            touchSleepWakeUpEnable(config.touchGpio, config.touchThreshold);
            touchArmed = true;
        } else {
            LOGW("[Power] Touch wake GPIO%u is outside allowed range GPIO%u-GPIO%u",
                 static_cast<unsigned>(config.touchGpio),
                 static_cast<unsigned>(POWER::TOUCH_WAKE_GPIO_MIN),
                 static_cast<unsigned>(POWER::TOUCH_WAKE_GPIO_MAX));
        }
    }
#else
    if (config.touchEnabled) {
        LOGW("[Power] Touch wake requested but touch sensor is not supported on this build");
    }
#endif

    if (timerArmed || buttonArmed || touchArmed) {
        LOGI("[Power] Wake sources armed: timer=%s(%lus) button=%s touch=%s(GPIO%u thr=%u)",
             timerArmed ? "ON" : "OFF",
             static_cast<unsigned long>(config.wakeIntervalMs / 1000UL),
             buttonArmed ? "ON" : "OFF",
             touchArmed ? "ON" : "OFF",
             static_cast<unsigned>(config.touchGpio),
             static_cast<unsigned>(config.touchThreshold));
        return true;
    } else {
        LOGE("[Power] No configured wake source armed; enabling fallback timer wake");
        if (armTimerWake(POWER::WAKE_INTERVAL_MIN_MS)) {
            LOGW("[Power] Fallback timer wake armed for %lus",
                 static_cast<unsigned long>(POWER::WAKE_INTERVAL_MIN_MS / 1000UL));
            return true;
        }
    }

    LOGE("[Power] No wake source armed; aborting deep sleep to avoid unrecoverable sleep");
    return false;
}

void PowerWakeController::setConfigureWakeSourcesCallback(void (*callback)()) {
    _configureWakeSourcesCallback = callback;
}

void PowerWakeController::resetTestHooks() {
    _configureWakeSourcesCallback = nullptr;
}

} // namespace POWER
