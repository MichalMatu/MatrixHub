#include "HeartbeatApiService.h"

#include "../../config/System.h"
#include "../../system/health/heartbeat/Heartbeat.h"
#include "../common/TestRequestRateLimiter.h"
#include <PsychicJson.h>
#include <utils/ResponseUtils.h>

namespace API {

namespace {

uint8_t countEnabledHeartbeatSlots(const RTC::HeartbeatData& cfg) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
        if (cfg.slots[i].enabled && cfg.slots[i].url[0] != '\0') {
            count++;
        }
    }
    return count;
}

void writeHeartbeatTestDiagnostics(JsonVariant& root, bool success, const char* status, uint8_t activeSlots) {
    root["success"] = success;
    root["status"] = status;
    root["message"] = status;
    root["active_slots"] = activeSlots;
    root["retry_count"] = HEARTBEAT::HTTP_RETRIES;
    root["timeout_ms"] = HEARTBEAT::HTTP_TIMEOUT_MS;
}

}  // namespace

HeartbeatApiService::HeartbeatApiService(PsychicHttpServer* server,
                                         SecurityManager* securityManager,
                                         POWER::PowerManager* powerManager,
                                         SYSTEM::HeartbeatSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "api/heartbeat"),
      _settings(settings) {
    // Heartbeat settings now come from ServiceRegistry. The API stays focused
    // on transport while the settings service retains ownership of persistence
    // and post-save apply logic.
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::HeartbeatData>>(
            SYSTEM::HeartbeatSettingsService::readState,
            SYSTEM::HeartbeatSettingsService::updateState,
            _settings,
            _server,
            "/api/heartbeat",
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            nullptr,
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void HeartbeatApiService::begin() {
    if (_configEndpoint) {
        _configEndpoint->begin();
    }
    
    // POST /api/heartbeat/test
    _server->on("/api/heartbeat/test", HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) -> esp_err_t {
            RTC::HeartbeatData cfg = SYSTEM::HEARTBEAT_CONFIG::copy();
            const uint8_t activeSlots = countEnabledHeartbeatSlots(cfg);
            
            if (activeSlots == 0) {
                return Response::error(request, 400, "heartbeat/no_enabled_slots", [](JsonVariant& root) {
                    writeHeartbeatTestDiagnostics(root, false, "no_enabled_slots", 0);
                });
            }

            if (!TESTS::testRequestRateLimiter().tryAcquire()) {
                const uint32_t retryAfterMs = TESTS::testRequestRateLimiter().remainingMs();
                return Response::error(request, 429, TESTS::kBusyErrorCode, [retryAfterMs](JsonVariant& root) {
                    root["success"] = false;
                    root["message"] = TESTS::kBusyErrorCode;
                    root["retry_after_ms"] = retryAfterMs;
                });
            }
            
            if (SYSTEM::Heartbeat::pingNow()) {
                return Response::success(request, [activeSlots](JsonVariant& root) {
                    writeHeartbeatTestDiagnostics(root, true, "queued", activeSlots);
                    root["message"] = "ping_triggered";
                });
            } else {
                return Response::error(request, 400, "heartbeat/ping_failed", [activeSlots](JsonVariant& root) {
                    writeHeartbeatTestDiagnostics(root, false, "ping_failed", activeSlots);
                });
            }
        })
    );
}

}  // namespace API
