/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <services/SleepService.h>
#include <WiFi.h>
#include <ESPmDNS.h>

// Definition of static member variables
sleepCallback SleepService::_sleepCallback = nullptr;
u_int64_t _wakeUpPin = WAKEUP_PIN_NUMBER;
bool _wakeUpSignal = WAKEUP_SIGNAL;
pinTermination _wakeUpTermination = pinTermination::FLOATING;

SleepService::SleepService(PsychicHttpServer *server,
                           SecurityManager *securityManager) : _server(server),
                                                               _securityManager(securityManager)
{
}

void SleepService::begin()
{
// OPTIONS (for CORS preflight)
#ifdef ENABLE_CORS
    _server->on(SLEEP_SERVICE_PATH,
                HTTP_OPTIONS,
                _securityManager->wrapRequest(
                    [this](PsychicRequest *request)
                    {
                        return request->reply(200);
                    },
                    AuthenticationPredicates::IS_AUTHENTICATED));
#endif

    _server->on(SLEEP_SERVICE_PATH,
                HTTP_POST,
                _securityManager->wrapRequest([this](PsychicRequest *request)
                                              { return this->sleep(request); },
                                              AuthenticationPredicates::IS_ADMIN));

    ESP_LOGV(SVK_TAG, "Registered POST endpoint: %s", SLEEP_SERVICE_PATH);
}

esp_err_t SleepService::sleep(PsychicRequest *request)
{
    request->reply(200);
    sleepNow();

    return ESP_OK;
}

void SleepService::sleepNow()
{
    ESP_LOGI(SVK_TAG, "Going into deep sleep now");

    // Callback for main code sleep preparation
    if (_sleepCallback)
    {
        _sleepCallback();
    }

    MDNS.end();
    vTaskDelay(pdMS_TO_TICKS(100));
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(200));

    // set pin function of _wakeUpPin
    pinMode(_wakeUpPin, INPUT);

    ESP_LOGD(SVK_TAG, "Enabling GPIO wakeup on pin GPIO%d with level %d\n", _wakeUpPin, _wakeUpSignal);
    ESP_LOGD(SVK_TAG, "Current level on GPIO%d: %d\n", _wakeUpPin, digitalRead(_wakeUpPin));

// special treatment for ESP32-C3 / C6 because of the RISC-V architecture
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32C6)
    esp_deep_sleep_enable_gpio_wakeup(BIT(_wakeUpPin), (esp_deepsleep_gpio_wake_up_mode_t)_wakeUpSignal);
#else
    esp_sleep_enable_ext1_wakeup(BIT(_wakeUpPin), (esp_sleep_ext1_wakeup_mode_t)_wakeUpSignal);

    switch (_wakeUpTermination)
    {
    case pinTermination::PULL_DOWN:
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        rtc_gpio_init((gpio_num_t)_wakeUpPin);
        rtc_gpio_pullup_dis((gpio_num_t)_wakeUpPin);
        rtc_gpio_pulldown_en((gpio_num_t)_wakeUpPin);
        break;
    case pinTermination::PULL_UP:
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
        rtc_gpio_init((gpio_num_t)_wakeUpPin);
        rtc_gpio_pullup_en((gpio_num_t)_wakeUpPin);
        rtc_gpio_pulldown_dis((gpio_num_t)_wakeUpPin);
        break;
    default:
        esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_AUTO);
    }
#endif

    ESP_LOGI(SVK_TAG, "Goodbye!");

    esp_deep_sleep_start();
}

void SleepService::setWakeUpPin(int pin, bool level, pinTermination termination)
{
    _wakeUpPin = (u_int64_t)pin;
    _wakeUpSignal = level;
    _wakeUpTermination = termination;
}
