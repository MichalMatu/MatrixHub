/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <services/RestartService.h>
#include <WiFi.h>
#include <ESPmDNS.h>

// Optional application-level restart hook.
// If provided by the application, it replaces the framework's legacy
// "disconnect WiFi and reboot" path with a firmware-defined restart flow.
//
// MatrixHub uses this to route UI/settings-triggered restarts through the same
// graceful shutdown sequence that hygiene sleep and factory reset already use:
// stop background tasks, persist shutdown reason, tear down networking cleanly,
// then call esp_restart().
extern "C" void svk_appRestartNow() __attribute__((weak));

// Native tests use the inline fallback from the header so they can link restart
// scheduling paths without pulling in the firmware-only timer implementation.
#if !defined(NATIVE_BUILD)

RestartService::RestartService(PsychicHttpServer *server, SecurityManager *securityManager) : _server(server),
                                                                                              _securityManager(securityManager)
{
}

void RestartService::begin()
{
    _server->on(RESTART_SERVICE_PATH,
                HTTP_POST,
                _securityManager->wrapRequest([this](PsychicRequest *request)
                                              { return this->restart(request); },
                                              AuthenticationPredicates::IS_ADMIN));

    ESP_LOGV(SVK_TAG, "Registered POST endpoint: %s", RESTART_SERVICE_PATH);
}

esp_err_t RestartService::restart(PsychicRequest *request)
{
    request->reply(200);
    restartNow();
    return ESP_OK;
}

void RestartService::restartNow()
{
    _restartPending = true;

    // Prefer the application-defined restart path when available.
    // This keeps framework-owned restart entry points aligned with the
    // firmware's newer shutdown orchestration instead of bypassing it with the
    // old "MDNS.end() + WiFi.disconnect() + ESP.restart()" sequence.
    if (svk_appRestartNow)
    {
        ESP_LOGI("RestartService", "Delegating restart to application hook");
        svk_appRestartNow();
        ESP_LOGE("RestartService", "Application restart hook returned; falling back to framework restart");
    }

    ESP_LOGI("RestartService", "Restarting system now...");
    vTaskDelay(pdMS_TO_TICKS(250));
    MDNS.end();
    vTaskDelay(pdMS_TO_TICKS(100));
    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP.restart();
}

void RestartService::scheduleRestart(uint32_t delayMs)
{
    if (_restartPending.exchange(true)) {
        ESP_LOGW("RestartService", "Restart already scheduled, ignoring duplicate request");
        return;
    }

    const uint32_t safeDelayMs = delayMs == 0 ? 1 : delayMs;
    TickType_t timerTicks = pdMS_TO_TICKS(safeDelayMs);
    if (timerTicks == 0) {
        timerTicks = 1;
    }
    ESP_LOGI("RestartService", "Scheduling restart in %lu ms", safeDelayMs);

    // Create timer on first use (lazy initialization)
    if (_restartTimer == nullptr) {
        _restartTimer = xTimerCreate(
            "RestartTimer",
            timerTicks,
            pdFALSE,  // One-shot
            nullptr,
            [](TimerHandle_t) {
                restartNow();
            }
        );
        if (_restartTimer == nullptr) {
            ESP_LOGE("RestartService", "Failed to create restart timer");
            _restartPending = false;
            return;
        }
    } else if (xTimerChangePeriod(_restartTimer, timerTicks, 0) != pdPASS) {
        ESP_LOGE("RestartService", "Failed to update restart timer period");
        _restartPending = false;
        return;
    }

    if (xTimerStart(_restartTimer, 0) != pdPASS) {
        ESP_LOGE("RestartService", "Failed to start restart timer");
        _restartPending = false;
    }
}

bool RestartService::isRestartPending()
{
    return _restartPending;
}

#endif
