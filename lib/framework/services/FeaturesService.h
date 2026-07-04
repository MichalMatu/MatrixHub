/**
 * @file FeaturesService.h
 * @brief Minimal features endpoint for frontend compatibility
 */

#pragma once

#include <PsychicHttp.h>

#define FEATURES_SERVICE_PATH "/rest/features"

#ifndef APP_VERSION
#define APP_VERSION "2.0.0"
#endif

#ifndef APP_NAME
#define APP_NAME "MatrixHub"
#endif

#ifndef BUILD_TARGET
#define BUILD_TARGET "esp32s3"
#endif

class FeaturesService {
public:
    explicit FeaturesService(PsychicHttpServer *server) : _server(server) {}

    void begin() {
        _server->on(FEATURES_SERVICE_PATH, HTTP_GET, [this](PsychicRequest *request) {
            char json[256];
            snprintf(json, sizeof(json), 
                "{"
                "\"ntp\":true,"
                "\"sleep\":true,"
                "\"firmware_version\":\"%s\","
                "\"firmware_name\":\"%s\","
                "\"firmware_built_target\":\"%s\""
                "}",
                APP_VERSION,
                APP_NAME,
                BUILD_TARGET
            );
            return request->reply(200, "application/json", json);
        });
    }

private:
    PsychicHttpServer *_server;
};
