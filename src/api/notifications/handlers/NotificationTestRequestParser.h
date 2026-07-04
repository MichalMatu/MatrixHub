#pragma once

#include "../../../config/App.h"
#include "../../../system/errors/ErrorCodes.h"
#include "../../../system/memory/PsramAllocator.h"

#include <ArduinoJson.h>
#include <PsychicJson.h>
#include <cstring>
#include <utils/ResponseUtils.h>

namespace API::Handlers {

struct TelegramTestRequestData {
    const char* text = nullptr;
    size_t textLen = 0;
};

struct WebhookTestRequestData {
    const char* content = nullptr;
};

struct PushoverTestRequestData {
    const char* message = nullptr;
};

template <typename TDocument>
inline bool parseJsonBodyWithConfiguredError(PsychicRequest* request,
                                             TDocument& doc,
                                             bool configured,
                                             size_t limit) {
    if (request->contentLength() > limit) {
        Response::payloadTooLarge(request);
        return false;
    }

    DeserializationError err = deserializeJson(doc, request->body());
    if (err == DeserializationError::NoMemory || doc.overflowed()) {
        Response::error(request,
                        413,
                        ErrorCodes::Input::PAYLOAD_TOO_LARGE,
                        [configured](JsonVariant& root) { root["configured"] = configured; });
        return false;
    }
    if (err) {
        Response::invalidJson(
            request,
            [configured](JsonVariant& root) { root["configured"] = configured; },
            err.c_str());
        return false;
    }

    return true;
}

template <typename TDocument>
inline bool parseJsonBodyOrRespond(PsychicRequest* request, TDocument& doc, size_t limit) {
    if (request->contentLength() > limit) {
        Response::payloadTooLarge(request);
        return false;
    }

    DeserializationError err = deserializeJson(doc, request->body());
    if (err == DeserializationError::NoMemory || doc.overflowed()) {
        Response::payloadTooLarge(request);
        return false;
    }
    if (err) {
        Response::invalidJson(request, err.c_str());
        return false;
    }

    return true;
}

inline bool parseTelegramTestRequest(PsychicRequest* request,
                                     bool configured,
                                     SYSTEM::SpiRamJsonDocument& doc,
                                     TelegramTestRequestData& out) {
    if (!parseJsonBodyWithConfiguredError(
            request,
            doc,
            configured,
            LIMITS::API::JSON_DOC::NOTIFICATIONS_TELEGRAM_TEST)) {
        return false;
    }

    out.text = doc["text"].as<const char*>();
    if (!out.text || out.text[0] == '\0') {
        out.text = doc["message"].as<const char*>();
    }
    if (!out.text || out.text[0] == '\0') {
        Response::error(request,
                        400,
                        ErrorCodes::Input::EMPTY_TEXT,
                        [configured](JsonVariant& root) { root["configured"] = configured; });
        return false;
    }

    out.textLen = strlen(out.text);
    if (out.textLen > APP::NOTIFICATIONS::TELEGRAM_MAX_TEXT_LEN) {
        Response::error(request,
                        400,
                        ErrorCodes::Input::TEXT_TOO_LONG,
                        [configured](JsonVariant& root) {
                            root["configured"] = configured;
                            root["max_len"] = APP::NOTIFICATIONS::TELEGRAM_MAX_TEXT_LEN;
                        });
        return false;
    }

    return true;
}

inline bool parseWebhookTestRequest(PsychicRequest* request,
                                    bool configured,
                                    SYSTEM::SpiRamJsonDocument& doc,
                                    WebhookTestRequestData& out) {
    if (!parseJsonBodyWithConfiguredError(
            request,
            doc,
            configured,
            LIMITS::API::JSON_DOC::NOTIFICATIONS_WEBHOOK_TEST)) {
        return false;
    }

    out.content = doc["content"].as<const char*>();
    if (!out.content || out.content[0] == '\0') {
        out.content = "{\"event\":\"test\",\"source\":\"esp32\"}";
    }

    return true;
}

inline bool parsePushoverTestRequest(PsychicRequest* request,
                                     SYSTEM::SpiRamJsonDocument& doc,
                                     PushoverTestRequestData& out) {
    if (!parseJsonBodyOrRespond(request, doc, LIMITS::API::JSON_DOC::NOTIFICATIONS_PUSHOVER_TEST)) {
        return false;
    }

    out.message = doc["message"].as<const char*>();
    if (!out.message || out.message[0] == '\0') {
        out.message = "Test Message from ESP32";
    }

    return true;
}

}  // namespace API::Handlers
