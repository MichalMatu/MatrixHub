#include <security/AuthenticationService.h>
#include <utils/ResponseUtils.h>

#include "../../src/config/System.h"
#include "../../src/system/errors/ErrorCodes.h"
#include "../../src/system/logging/Logging.h"

#include <esp_heap_caps.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "Security"

namespace {

struct AsyncSignInJob {
    httpd_req_t* req = nullptr;
    SecurityManager* securityManager = nullptr;
    char* username = nullptr;
    char* password = nullptr;
};

QueueHandle_t sSignInQueue = nullptr;
TaskHandle_t sSignInWorkerHandle = nullptr;

constexpr const char* kJsonMimeType = "application/json";
constexpr const char* kStatusOk = "200 OK";
constexpr const char* kStatusUnauthorized = "401 Unauthorized";
constexpr const char* kStatusTooManyRequests = "429 Too Many Requests";
constexpr const char* kStatusServiceUnavailable = "503 Service Unavailable";

void freeAsyncSignInJob(AsyncSignInJob& job) {
    if (job.username) {
        heap_caps_free(job.username);
        job.username = nullptr;
    }
    if (job.password) {
        heap_caps_free(job.password);
        job.password = nullptr;
    }
    job.req = nullptr;
    job.securityManager = nullptr;
}

char* duplicateField(const char* value) {
    const char* safeValue = value ? value : "";
    const size_t len = strlen(safeValue) + 1;
    char* copy = static_cast<char*>(heap_caps_malloc(len, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!copy) {
        copy = static_cast<char*>(heap_caps_malloc(len, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    if (!copy) {
        return nullptr;
    }
    memcpy(copy, safeValue, len);
    return copy;
}

esp_err_t sendAsyncJson(httpd_req_t* req, const char* status, const String& payload) {
    if (!req) {
        return ESP_ERR_INVALID_ARG;
    }

    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, kJsonMimeType);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, payload.c_str(), payload.length());
}

esp_err_t sendAsyncError(httpd_req_t* req, const char* status, const char* errorCode, const char* message = nullptr) {
    String payload = "{\"ok\":false,\"error\":\"";
    payload += errorCode ? errorCode : ErrorCodes::Internal::UNEXPECTED_ERROR;
    payload += "\"";
    if (message && message[0] != '\0') {
        payload += ",\"message\":\"";
        payload += message;
        payload += "\"";
    }
    payload += "}";
    return sendAsyncJson(req, status, payload);
}

void completeAsyncRequest(httpd_req_t* req) {
    if (!req) {
        return;
    }

    if (httpd_req_async_handler_complete(req) != ESP_OK) {
        LOGW("Failed to complete async sign-in request");
    }
}

void processAsyncSignIn(AsyncSignInJob& job) {
    if (!job.req || !job.securityManager) {
        return;
    }

    const uint32_t startMs = millis();
    Authentication authentication =
        job.securityManager->authenticate(job.username ? job.username : "",
                                          job.password ? job.password : "");

    esp_err_t responseErr = ESP_OK;
    if (authentication.authenticated && authentication.user) {
        const String token = job.securityManager->generateJWT(authentication.user.get());
        if (token.isEmpty()) {
            responseErr = sendAsyncError(job.req,
                                         kStatusServiceUnavailable,
                                         ErrorCodes::Service::UNAVAILABLE,
                                         "Token generation failed");
        } else {
            String payload = "{\"access_token\":\"";
            payload += token;
            payload += "\"}";
            responseErr = sendAsyncJson(job.req, kStatusOk, payload);
        }
    } else {
        responseErr = sendAsyncError(job.req, kStatusUnauthorized, ErrorCodes::Auth::INVALID_CREDENTIALS);
    }

    if (responseErr != ESP_OK) {
        LOGW("Async sign-in response failed: %s", esp_err_to_name(responseErr));
    }

    const uint32_t elapsedMs = millis() - startMs;
    if (elapsedMs >= 250) {
        LOGW("Async sign-in took %lu ms", static_cast<unsigned long>(elapsedMs));
    } else {
        LOGD("Async sign-in took %lu ms", static_cast<unsigned long>(elapsedMs));
    }
}

void asyncSignInWorkerTask(void* param) {
    (void)param;

    AsyncSignInJob job;
    while (true) {
        if (xQueueReceive(sSignInQueue, &job, portMAX_DELAY) == pdTRUE) {
            processAsyncSignIn(job);
            completeAsyncRequest(job.req);
            freeAsyncSignInJob(job);
        }
    }
}

bool ensureAsyncSignInWorkerStarted() {
    if (sSignInQueue && sSignInWorkerHandle) {
        return true;
    }

    if (!sSignInQueue) {
        sSignInQueue = xQueueCreate(NET::AUTH::SIGN_IN_ASYNC_QUEUE_DEPTH, sizeof(AsyncSignInJob));
        if (!sSignInQueue) {
            LOGE("Failed to create async sign-in queue");
            return false;
        }
    }

    if (!sSignInWorkerHandle) {
        const BaseType_t created = xTaskCreatePinnedToCore(
            asyncSignInWorkerTask,
            "AuthSignIn",
            NET::AUTH::SIGN_IN_ASYNC_STACK_BYTES,
            nullptr,
            NET::AUTH::SIGN_IN_ASYNC_TASK_PRIORITY,
            &sSignInWorkerHandle,
            CONFIG::TASKS::CORE_APP);
        if (created != pdPASS) {
            LOGE("Failed to start async sign-in worker");
            sSignInWorkerHandle = nullptr;
            return false;
        }
    }

    return true;
}

esp_err_t queueAsyncSignInRequest(PsychicRequest* request,
                                  SecurityManager* securityManager,
                                  const char* username,
                                  const char* password) {
    if (!request || !securityManager) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!ensureAsyncSignInWorkerStarted()) {
        return ESP_FAIL;
    }

    httpd_req_t* asyncReq = nullptr;
    esp_err_t err = httpd_req_async_handler_begin(request->request(), &asyncReq);
    if (err != ESP_OK) {
        LOGW("Failed to create async sign-in request: %s", esp_err_to_name(err));
        return err;
    }

    AsyncSignInJob job;
    job.req = asyncReq;
    job.securityManager = securityManager;
    job.username = duplicateField(username);
    job.password = duplicateField(password);
    if (!job.username || !job.password) {
        freeAsyncSignInJob(job);
        completeAsyncRequest(asyncReq);
        return ESP_ERR_NO_MEM;
    }

    if (xQueueSend(sSignInQueue, &job, 0) != pdTRUE) {
        freeAsyncSignInJob(job);
        completeAsyncRequest(asyncReq);
        LOGW("Async sign-in queue busy");
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

}  // namespace

AuthenticationService::AuthenticationService(PsychicHttpServer *server, SecurityManager *securityManager) : _server(server),
                                                                                                            _securityManager(securityManager)
{
}

void AuthenticationService::begin()
{
    (void)ensureAsyncSignInWorkerStarted();

    // Signs in a user if the username and password match. PBKDF2 verification is
    // dispatched off the single-threaded httpd task so HTTPS can keep accepting
    // and servicing other sockets while sign-in is in progress.
    _server->on(SIGN_IN_PATH, HTTP_POST, [this](PsychicRequest *request, JsonVariant &json)
                {
                    // DoS Protection: Reject large payloads early
                    if (request->contentLength() > NET::AUTH::SIGN_IN_MAX_PAYLOAD_BYTES) {
                         return Response::payloadTooLarge(request);
                    }

                    // Protection: Login Rate Limiting (3 attempts / 60s)
                    if (!_securityManager->checkLoginRateLimit(request)) {
                         return Response::error(request, 429, ErrorCodes::Auth::LOGIN_RATE_LIMITED, "Too Many Login Attempts");
                    }

                    if (!json.is<JsonObject>()) {
                        return Response::error(request, 401, ErrorCodes::Auth::INVALID_CREDENTIALS);
                    }

                    const char* username = json["username"] | "";
                    const char* password = json["password"] | "";
                    const esp_err_t queueErr =
                        queueAsyncSignInRequest(request, _securityManager, username, password);
                    if (queueErr == ESP_OK) {
                        return ESP_OK;
                    }

                    if (queueErr == ESP_ERR_TIMEOUT) {
                        return Response::error(request,
                                               503,
                                               ErrorCodes::Busy::RESOURCE_LOCKED,
                                               "Sign-in is already in progress");
                    }

                    return Response::error(request, 503, ErrorCodes::Service::UNAVAILABLE);
                });

    LOGV("Registered POST endpoint: %s", SIGN_IN_PATH);

    // Verifies that the request supplied a valid JWT
    _server->on(VERIFY_AUTHORIZATION_PATH, HTTP_GET, [this](PsychicRequest *request)
                {
        Authentication authentication = _securityManager->authenticateRequest(request);
        if (authentication.authenticated) {
            return request->reply(200);
        }
        return Response::error(request, 401, ErrorCodes::Auth::UNAUTHORIZED); });

    LOGV("Registered GET endpoint: %s", VERIFY_AUTHORIZATION_PATH);
}
