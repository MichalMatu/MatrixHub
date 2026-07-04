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

#include <services/FactoryResetService.h>

// Optional application-level hook.
// If provided by the application, it will be used instead of the framework's
// default config-directory wipe.
extern "C" void svk_appFactoryReset() __attribute__((weak));

using namespace std::placeholders;

FactoryResetService::FactoryResetService(PsychicHttpServer *server,
                                         FS *fs,
                                         SecurityManager *securityManager) : _server(server),
                                                                             fs(fs),
                                                                             _securityManager(securityManager)
{
}

void FactoryResetService::begin()
{
    _server->on(FACTORY_RESET_SERVICE_PATH,
                HTTP_POST,
                _securityManager->wrapRequest([this](PsychicRequest *request)
                                              { return this->handleRequest(request); },
                                              AuthenticationPredicates::IS_ADMIN));

    ESP_LOGV(SVK_TAG, "Registered POST endpoint: %s", FACTORY_RESET_SERVICE_PATH);
}

esp_err_t FactoryResetService::handleRequest(PsychicRequest *request)
{
    request->reply(200);
    factoryReset();

    return ESP_OK;
}

/**
 * Delete function assumes that all files are stored flat, within the config directory.
 */
void FactoryResetService::factoryReset()
{
    // Prefer application-defined factory reset, if present.
    // This allows projects to extend the reset (e.g. clear calibration prefs,
    // format FS, etc.) without forking the framework service route.
    if (svk_appFactoryReset)
    {
        ESP_LOGI(SVK_TAG, "Delegating factory reset to application hook");
        svk_appFactoryReset();
        return;
    }

    File root = fs->open(FS_CONFIG_DIRECTORY);
    File file;
    while (file = root.openNextFile())
    {
        String path = file.path();
        file.close();
        fs->remove(path);
    }
    RestartService::restartNow();
}
