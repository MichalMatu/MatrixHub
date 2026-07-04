#pragma once

#include <PsychicHttpServer.h>

#include "../NotificationTestJobScheduler.h"
#include "../../../system/errors/ErrorCodes.h"
#include "../../common/TestRequestRateLimiter.h"

#include <ArduinoJson.h>
#include <utils/ResponseUtils.h>

namespace API::Handlers {

struct NotificationTestScheduleResponseOptions {
    const char* busyErrorCode = ErrorCodes::Busy::RESOURCE_LOCKED;
    bool configured = false;
    bool includeConfigured = false;
    bool includeConfiguredOnRateLimit = false;
};

inline esp_err_t respondQueued(PsychicRequest* request) {
    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
        root["status"] = "queued";
    });
}

inline esp_err_t respondScheduleResult(PsychicRequest* request,
                                       const NotificationTestScheduleResult& result,
                                       const NotificationTestScheduleResponseOptions& options) {
    const auto withConfigured = [&options](JsonVariant& root) {
        if (options.includeConfigured) {
            root["configured"] = options.configured;
        }
    };

    switch (result.status) {
        case NotificationTestScheduleStatus::Queued:
            return respondQueued(request);
        case NotificationTestScheduleStatus::ServiceUnavailable:
            return Response::error(request, 503, ErrorCodes::Service::UNAVAILABLE, withConfigured);
        case NotificationTestScheduleStatus::Busy:
            return Response::error(request, 429, options.busyErrorCode, withConfigured);
        case NotificationTestScheduleStatus::RateLimited:
            return Response::error(request,
                                   429,
                                   TESTS::kBusyErrorCode,
                                   [&options, retryAfterMs = result.retryAfterMs](JsonVariant& root) {
                                       if (options.includeConfigured || options.includeConfiguredOnRateLimit) {
                                           root["configured"] = options.configured;
                                       }
                                       root["success"] = false;
                                       root["retry_after_ms"] = retryAfterMs;
                                   });
        case NotificationTestScheduleStatus::AllocFailed:
            return Response::error(request, 500, ErrorCodes::Internal::ALLOC_FAILED, withConfigured);
        case NotificationTestScheduleStatus::TaskCreateFailed:
            return Response::error(request, 500, ErrorCodes::Internal::TASK_CREATE_FAILED, withConfigured);
        default:
            return Response::error(request, 500, ErrorCodes::Service::UNAVAILABLE, withConfigured);
    }
}

}  // namespace API::Handlers
