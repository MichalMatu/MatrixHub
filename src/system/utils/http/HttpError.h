#pragma once

#include <PsychicHttp.h>
#include <WString.h>
#include <cstdio>

namespace API {

/**
 * @brief Centralized HTTP JSON error response utility.
 *
 * Uses PsychicHttp's native request->reply() for compatibility with all
 * handler contexts, including multipart upload chunk callbacks.
 *
 * Produces: {"ok":false,"error":"<errorCode>"} or
 *           {"ok":false,"error":"<errorCode>","message":"<escapedMessage>"}
 */
class HttpError {
 public:
  /**
   * @brief Sends a standardized JSON error response
   *
   * @param request The active PsychicHttp request
   * @param statusCode HTTP status code (e.g., 400, 403, 404, 413, 500, 503, 507)
   * @param errorCode Machine-readable error code (e.g., "fs/file_not_found")
   * @param message Optional human-readable message for UI display (auto-escaped for JSON safety)
   * @return esp_err_t result of the reply operation
   */
  static esp_err_t send(PsychicRequest* request, int statusCode, const char* errorCode, const char* message = nullptr) {
    if (!request) return ESP_FAIL;

    char body[384];
    if (message) {
      char escaped[192];
      escapeJson(message, escaped, sizeof(escaped));
      snprintf(body, sizeof(body),
        "{\"ok\":false,\"error\":\"%s\",\"message\":\"%s\"}", errorCode, escaped);
    } else {
      snprintf(body, sizeof(body),
        "{\"ok\":false,\"error\":\"%s\"}", errorCode);
    }

    return request->reply(statusCode, "application/json", body);
  }

 private:
  /**
   * @brief Escapes a C string for safe embedding inside a JSON string value.
   *
   * Handles: " -> \"  |  \ -> \\  |  control chars (0x00-0x1F) -> skipped
   */
  static void escapeJson(const char* src, char* dst, size_t dstSize) {
    if (!src || dstSize < 2) { dst[0] = '\0'; return; }

    size_t j = 0;
    for (size_t i = 0; src[i] && j < dstSize - 2; ++i) {
      char c = src[i];
      if (c == '"' || c == '\\') {
        if (j + 2 >= dstSize - 1) break;
        dst[j++] = '\\';
        dst[j++] = c;
      } else if (c >= 0x20) {
        dst[j++] = c;
      }
      // Skip control characters silently
    }
    dst[j] = '\0';
  }
};

}  // namespace API
