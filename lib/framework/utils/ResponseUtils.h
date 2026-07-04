/**
 * @file ResponseUtils.h
 * @brief DRY utilities for HTTP JSON responses
 *
 * Provides helper functions for common response patterns to reduce code duplication:
 * - Error responses with standard format
 * - Success responses with JSON builder callback
 * - JSON parse error handling
 * - Boolean to JSON string conversion
 *
 * Usage example:
 * @code
 * #include <utils/ResponseUtils.h>
 *
 * // Error response
 * return Response::error(request, 400, ErrorCodes::Input::EMPTY_TEXT);
 *
 * // Success response with builder
 * return Response::success(request, [&](JsonVariant& root) {
 *     root["ok"] = true;
 *     root["data"] = someValue;
 * });
 *
 * // Parse JSON body with automatic error handling
 * JsonDocument doc;
 * if (auto err = Response::parseJsonBody(request, doc, Response::kDefaultJsonPayloadLimitBytes); err != ESP_OK) {
 *     return err;  // Error response already sent
 * }
 * @endcode
 */

#pragma once

#include <ArduinoJson.h>
#include <PsychicJson.h>
#include <PsychicRequest.h>
#include <functional>

#include "../../../src/system/errors/ErrorCodes.h"

namespace Response {

inline constexpr size_t kDefaultJsonPayloadLimitBytes = 16 * 1024;
inline constexpr size_t kStateJsonPayloadLimitBytes = 8 * 1024;
inline constexpr size_t kJsonAuthCallbackPayloadLimitBytes = 8 * 1024;

/**
 * Send a JSON error response
 * 
 * Response format: {"ok": false, "error": "<errorCode>"}
 * 
 * @param request The HTTP request to respond to
 * @param httpCode HTTP status code (e.g., 400, 404, 500)
 * @param errorCode Error code string (use ErrorCodes:: constants)
 * @return esp_err_t from response.send()
 */
inline esp_err_t error(PsychicRequest* request, int httpCode, const char* errorCode) {
    PsychicJsonResponse response(request);
    response.setCode(httpCode);
    JsonVariant& root = response.getRoot();
    root["ok"] = false;
    root["error"] = errorCode;
    return response.send();
}

/**
 * Send a JSON error response with human-readable message
 * 
 * Response format: {"ok": false, "error": "<errorCode>", "message": "<message>"}
 * 
 * @param request The HTTP request to respond to
 * @param httpCode HTTP status code
 * @param errorCode Machine-readable error code string (use ErrorCodes:: constants)
 * @param message Human-readable message for UI display
 * @return esp_err_t from response.send()
 */
inline esp_err_t error(PsychicRequest* request, int httpCode, const char* errorCode,
                       const char* message) {
    PsychicJsonResponse response(request);
    response.setCode(httpCode);
    JsonVariant& root = response.getRoot();
    root["ok"] = false;
    root["error"] = errorCode;
    root["message"] = message;
    return response.send();
}

/**
 * Send a JSON error response with extra fields
 * 
 * Response format: {"ok": false, "error": "<errorCode>", ...extraFields}
 * 
 * @param request The HTTP request to respond to
 * @param httpCode HTTP status code
 * @param errorCode Error code string
 * @param extraBuilder Lambda to add extra fields to response
 * @return esp_err_t from response.send()
 */
inline esp_err_t error(PsychicRequest* request, int httpCode, const char* errorCode,
                       std::function<void(JsonVariant&)> extraBuilder) {
    PsychicJsonResponse response(request);
    response.setCode(httpCode);
    JsonVariant& root = response.getRoot();
    root["ok"] = false;
    root["error"] = errorCode;
    extraBuilder(root);
    return response.send();
}

inline esp_err_t payloadTooLarge(PsychicRequest* request) {
    return Response::error(request, 413, ErrorCodes::Input::PAYLOAD_TOO_LARGE);
}

inline esp_err_t invalidJson(PsychicRequest* request, const char* detail = nullptr) {
    PsychicJsonResponse response(request);
    response.setCode(400);
    JsonVariant& root = response.getRoot();
    root["ok"] = false;
    root["error"] = ErrorCodes::Input::INVALID_JSON;
    if (detail && detail[0] != '\0') {
        root["detail"] = detail;
    }
    return response.send();
}

inline esp_err_t invalidJson(PsychicRequest* request,
                             std::function<void(JsonVariant&)> extraBuilder,
                             const char* detail = nullptr) {
    PsychicJsonResponse response(request);
    response.setCode(400);
    JsonVariant& root = response.getRoot();
    root["ok"] = false;
    root["error"] = ErrorCodes::Input::INVALID_JSON;
    if (detail && detail[0] != '\0') {
        root["detail"] = detail;
    }
    extraBuilder(root);
    return response.send();
}

/**
 * Send a JSON success response with builder callback
 * 
 * The builder callback receives the root JsonVariant to populate.
 * Caller should set "ok" = true if desired.
 * 
 * @param request The HTTP request to respond to
 * @param builder Lambda that populates the JSON root
 * @param httpCode HTTP status code (default 200)
 * @return esp_err_t from response.send()
 */
inline esp_err_t success(PsychicRequest* request,
                         std::function<void(JsonVariant&)> builder,
                         int httpCode = 200) {
    PsychicJsonResponse response(request);
    response.setCode(httpCode);
    JsonVariant& root = response.getRoot();
    builder(root);
    return response.send();
}

/**
 * Parse JSON from request body with automatic error response
 * 
 * If parsing fails, or payload exceeds limit, sends an error response and returns the error.
 * If parsing succeeds, returns ESP_OK.
 * 
 * @param request The HTTP request containing JSON body
 * @param doc JsonDocument to deserialize into
 * @param limit Maximum allowed payload size.
 *              NOTE: Body is explicitly allocated in PSRAM via PsychicRequest.
 * @return ESP_OK on success, or error code if parsing failed (response already sent)
 */
template <typename TDocument>
inline esp_err_t parseJsonBody(PsychicRequest* request, TDocument& doc, size_t limit) {
    if (request->contentLength() > limit) {
        return payloadTooLarge(request);
    }
    
    DeserializationError err = deserializeJson(doc, request->body());
    if (err == DeserializationError::NoMemory || doc.overflowed()) {
        return payloadTooLarge(request);
    }
    if (err) {
        return invalidJson(request, err.c_str());
    }
    return ESP_OK;
}

/**
 * Parse JSON from request body with configured flag in error response
 * 
 * Like parseJsonBody but includes "configured" field in error response.
 * Useful for test endpoints that always need to report configuration status.
 * 
 * @param request The HTTP request containing JSON body
 * @param doc JsonDocument to deserialize into
 * @param configured Whether the feature is configured (added to error response)
 * @param limit Maximum allowed payload size
 * @return ESP_OK on success, or error code if parsing failed (response already sent)
 */
template <typename TDocument>
inline esp_err_t parseJsonBodyWithConfig(PsychicRequest* request, TDocument& doc, bool configured, size_t limit) {
    if (request->contentLength() > limit) {
        return payloadTooLarge(request);
    }

    DeserializationError err = deserializeJson(doc, request->body());
    if (err == DeserializationError::NoMemory || doc.overflowed()) {
        return Response::error(request,
                               413,
                               ErrorCodes::Input::PAYLOAD_TOO_LARGE,
                               [configured](JsonVariant& root) { root["configured"] = configured; });
    }
    if (err) {
        return invalidJson(
            request,
            [configured](JsonVariant& root) { root["configured"] = configured; },
            err.c_str());
    }
    return ESP_OK;
}

/**
 * Convert boolean to JSON string literal
 * 
 * Useful for manual JSON string building where you need "true"/"false" literals.
 * 
 * @param val Boolean value
 * @return "true" or "false" string literal
 */
inline constexpr const char* jsonBool(bool val) {
    return val ? "true" : "false";
}

}  // namespace Response
