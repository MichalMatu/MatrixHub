#ifndef RestartService_h
#define RestartService_h

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

#include <PsychicHttp.h>
#include <security/SecurityManager.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <atomic>

#define RESTART_SERVICE_PATH "/rest/restart"

class RestartService
{
public:
#if defined(NATIVE_BUILD)
    // Native tests do not link the firmware restart service object. Keep a tiny
    // inline fallback here so services can still exercise "restart required" logic.
    RestartService(PsychicHttpServer *server, SecurityManager *securityManager)
        : _server(server), _securityManager(securityManager) {}

    void begin() {}

    static void restartNow() {
        _restartPending = false;
    }

    static void scheduleRestart(uint32_t delayMs = 1500) {
        (void)delayMs;
        _restartPending = true;
    }

    static bool isRestartPending() {
        return _restartPending.load();
    }
#else
    RestartService(PsychicHttpServer *server, SecurityManager *securityManager);

    void begin();

    /**
     * Restart immediately (blocking).
     * Gracefully disconnects WiFi and mDNS before calling ESP.restart().
     */
    static void restartNow();

    /**
     * Schedule a restart after a delay.
     * Uses FreeRTOS one-shot timer - safe to call from HTTP handlers.
     * Multiple calls are ignored if a restart is already pending.
     * 
     * @param delayMs Delay before restart in milliseconds (default 1500ms)
     */
    static void scheduleRestart(uint32_t delayMs = 1500);

    /**
     * Check if a restart is currently scheduled.
     */
    static bool isRestartPending();
#endif

private:
    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
#if defined(NATIVE_BUILD)
    // Native tests never execute the real HTTP restart endpoint; they only need
    // a stub symbol so service code that references restart paths still links.
    esp_err_t restart(PsychicRequest *request) {
        (void)request;
        return ESP_OK;
    }
#else
    esp_err_t restart(PsychicRequest *request);
#endif

    // Inline statics prevent duplicate definitions when native tests include
    // multiple service .cpp files directly into one translation unit.
    inline static TimerHandle_t _restartTimer = nullptr;
    inline static std::atomic<bool> _restartPending{false};
};

#endif // end RestartService_h
