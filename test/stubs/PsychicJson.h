#pragma once

#include <ArduinoJson.h>
#include <string>
#include <vector>
#include "PsychicRequest.h"

class PsychicJsonResponse {
public:
#if ARDUINOJSON_VERSION_MAJOR == 6
    explicit PsychicJsonResponse(PsychicRequest *request, bool isArray = false)
        : _request(request), _doc(kJsonBufferSize) {
        if (isArray) {
            _root = _doc.createNestedArray();
        } else {
            _root = _doc.createNestedObject();
        }
    }
#else
    explicit PsychicJsonResponse(PsychicRequest *request, bool isArray = false)
        : _request(request) {
        if (isArray) {
            _root = _doc.add<JsonArray>();
        } else {
            _root = _doc.add<JsonObject>();
        }
    }
#endif

    void setCode(int code) { _code = code; }
    JsonVariant& getRoot() { return _root; }

    esp_err_t send() {
        if (_request) {
            _request->lastStatusCode = _code;
            _request->lastResponseBody.clear();

            const size_t payloadSize = measureJson(_root);
            std::vector<char> payload(payloadSize + 1, '\0');
            serializeJson(_root, payload.data(), payload.size());
            _request->lastResponseBody.assign(payload.data(), payloadSize);
        }
        return ESP_OK;
    }

private:
    static constexpr size_t kJsonBufferSize = 2048;

    PsychicRequest *_request = nullptr;
    int _code = 200;
#if ARDUINOJSON_VERSION_MAJOR == 6
    DynamicJsonDocument _doc;
#else
    JsonDocument _doc;
#endif
    JsonVariant _root;
};
