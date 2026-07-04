/**
 * @file UdpApiService.cpp
 * @brief Implementation of UDP pusher API
 */

#include "UdpApiService.h"

#include "../../config/json/SystemConfigJson.h"
#include "../../udp/UdpPusher.h"
#include "../../udp/UdpSettingsService.h"
#include "../common/TestRequestRateLimiter.h"
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <PsychicJson.h>
#include <utils/ResponseUtils.h>
#include <cstring>

namespace API {

namespace {
constexpr const char* kUdpConfigPath = "/api/udp";

bool isPrintableHost(const char* host) {
    if (!host || host[0] == '\0') {
        return false;
    }
    const size_t len = strlen(host);
    if (len >= sizeof(RTC::UdpPusherData::host)) {
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        const unsigned char c = static_cast<unsigned char>(host[i]);
        if (c <= 32 || c == 127) {
            return false;
        }
    }
    return true;
}

bool jsonPortOutOfRange(JsonObject& root) {
    JsonVariant portValue = root[CONFIG::Keys::kPort];
    if (portValue.isNull()) {
        return false;
    }
    if (!portValue.is<uint32_t>()) {
        return true;
    }
    const uint32_t port = portValue.as<uint32_t>();
    return port < LIMITS::UDP_PUSHER::MIN_PORT || port > LIMITS::UDP_PUSHER::MAX_PORT;
}

esp_err_t sendUdpTestResult(PsychicRequest* request, UDPPUSH::UdpPusher::PushNowResult result) {
    switch (result) {
        case UDPPUSH::UdpPusher::PushNowResult::Queued:
            return Response::success(request, [](JsonVariant& root) {
                root["success"] = true;
                root["status"] = "queued";
                root["message"] = "UDP packet queued";
            });
        case UDPPUSH::UdpPusher::PushNowResult::Sent:
            return Response::success(request, [](JsonVariant& root) {
                root["success"] = true;
                root["status"] = "sent";
                root["message"] = "UDP packet sent";
            });
        case UDPPUSH::UdpPusher::PushNowResult::NotConfigured:
            return Response::error(request, 400, "config/not_configured", [](JsonVariant& root) {
                root["success"] = false;
                root["status"] = "not_configured";
                root["message"] = "UDP not configured";
            });
        case UDPPUSH::UdpPusher::PushNowResult::WorkerStopping:
            return Response::error(request, 503, "udp/worker_stopping", [](JsonVariant& root) {
                root["success"] = false;
                root["status"] = "worker_stopping";
                root["message"] = "UDP worker is stopping";
            });
        case UDPPUSH::UdpPusher::PushNowResult::WifiDisconnected:
            return Response::error(request, 503, "network/wifi_disconnected", [](JsonVariant& root) {
                root["success"] = false;
                root["status"] = "wifi_disconnected";
                root["message"] = "WiFi not connected";
            });
        case UDPPUSH::UdpPusher::PushNowResult::SendFailed:
        default:
            return Response::error(request, 502, "udp/send_failed", [](JsonVariant& root) {
                root["success"] = false;
                root["status"] = "send_failed";
                root["message"] = "UDP send failed";
            });
    }
}
}

UdpApiService::UdpApiService(PsychicHttpServer* server,
                             SecurityManager* securityManager,
                             POWER::PowerManager* powerManager,
                             UDPPUSH::UdpPusher* udpPusher,
                             UDPPUSH::UdpSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "api/udp"),
      _udpPusher(udpPusher),
      _settings(settings) {
    // Registry owns persistent UDP config; this API now only exposes the HTTP
    // transport and on-demand test send.
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::UdpPusherData>>(
            UDPPUSH::UdpSettingsService::readState,
            UDPPUSH::UdpSettingsService::updateState,
            _settings,
            _server,
            kUdpConfigPath,
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            [this](PsychicRequest* request, JsonObject& jsonObject) {
                return validateConfigUpdate(request, jsonObject);
            },
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void UdpApiService::begin() {
    if (_configEndpoint) {
        _configEndpoint->begin();
    }
    
    // POST /api/udp/test - trigger immediate UDP push
    _server->on("/api/udp/test", HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) -> esp_err_t {
            // Check if UDP is configured
            RTC::UdpPusherData cfg = RTC::copyConfigSection(&RTC::ConfigStore::udpPusher);
            if (!cfg.enabled || cfg.host[0] == '\0' || cfg.port == 0) {
                return Response::error(request, 400, "config/not_configured", [](JsonVariant& root) {
                    root["success"] = false;
                    root["message"] = "UDP not configured";
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
            
            if (!_udpPusher) {
                return Response::error(request, 503, "udp/unavailable", [](JsonVariant& root) {
                    root["success"] = false;
                    root["status"] = "unavailable";
                    root["message"] = "UDP pusher unavailable";
                });
            }

            return sendUdpTestResult(request, _udpPusher->pushNow());
        })
    );
}

StateHandlerResult UdpApiService::validateConfigUpdate(PsychicRequest* request, JsonObject& jsonObject) {
    (void)request;

    if (jsonPortOutOfRange(jsonObject)) {
        return StateHandlerResult::failure("input/invalid_port", 400);
    }

    RTC::UdpPusherData nextState = RTC::copyConfigSection(&RTC::ConfigStore::udpPusher);
    CONFIG::JSON::deserializeUdpPusher(jsonObject, nextState);

    if (!nextState.enabled) {
        return StateHandlerResult::success();
    }

    if (!isPrintableHost(nextState.host)) {
        return StateHandlerResult::failure("input/invalid_host", 400);
    }
    if (nextState.port < LIMITS::UDP_PUSHER::MIN_PORT || nextState.port > LIMITS::UDP_PUSHER::MAX_PORT) {
        return StateHandlerResult::failure("input/invalid_port", 400);
    }

    return StateHandlerResult::success();
}

}  // namespace API
