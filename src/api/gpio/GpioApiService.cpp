#include "GpioApiService.h"

#include <ArduinoJson.h>
#include <utils/ResponseUtils.h>

#include "../../config/App.h"
#include "../../config/json/GpioConfigJson.h"
#include "../../gpio/GpioSafePins.h"
#include "../../gpio/GpioService.h"
#include "../../system/memory/PsramAllocator.h"
#include "../../system/utils/json/JsonResponseWriter.h"

namespace API {

namespace {

constexpr const char* kPinsPath = "/api/gpio/pins";
constexpr const char* kConfigPath = "/api/gpio/config";
constexpr const char* kStatusPath = "/api/gpio/status";
constexpr const char* kOutputPath = "/api/gpio/output";
constexpr size_t kGpioPayloadBytes = LIMITS::API::JSON_DOC::GPIO_CONFIG;

}  // namespace

GpioApiService::GpioApiService(PsychicHttpServer* server,
                               SecurityManager* securityManager,
                               POWER::PowerManager* powerManager,
                               GPIO::GpioService* service)
    : BaseApiService(server, securityManager, powerManager, "api/gpio"),
      _service(service) {}

void GpioApiService::begin() {
    _server->on(kPinsPath, HTTP_GET, wrapAuth([this](PsychicRequest* request) {
        return handlePins(request);
    }));
    _server->on(kConfigPath, HTTP_GET, wrapAuth([this](PsychicRequest* request) {
        return handleConfigGet(request);
    }));
    _server->on(kConfigPath, HTTP_POST, wrapAdmin([this](PsychicRequest* request) {
        return handleConfigPost(request);
    }));
    _server->on(kStatusPath, HTTP_GET, wrapAuth([this](PsychicRequest* request) {
        return handleStatus(request);
    }));
    _server->on(kOutputPath, HTTP_POST, wrapAdmin([this](PsychicRequest* request) {
        return handleOutput(request);
    }));
}

esp_err_t GpioApiService::handlePins(PsychicRequest* request) {
    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) {
        return ESP_FAIL;
    }

    w.raw("[");
    bool first = true;
    const auto* pins = GPIO::allowedPins();
    for (uint8_t i = 0; i < GPIO::allowedPinCount(); i++) {
        if (!first) {
            w.raw(",");
        }
        first = false;
        w.raw("{");
        w.key(CONFIG::Keys::kId); w.string(pins[i].id);
        w.raw(","); w.key(CONFIG::Keys::kName); w.string(pins[i].name);
        w.raw(","); w.key(CONFIG::Keys::kPin); w.value(static_cast<unsigned int>(pins[i].pin));
        w.raw(","); w.key(CONFIG::Keys::kAllowed); w.value(true);
        w.raw(","); w.key("input"); w.value(pins[i].allowInput);
        w.raw(","); w.key("output"); w.value(pins[i].allowOutput);
        w.raw(","); w.key("pull_up"); w.value(pins[i].allowPullup);
        w.raw(","); w.key("pull_down"); w.value(pins[i].allowPulldown);
        w.raw(","); w.key(CONFIG::Keys::kReason); w.string(pins[i].reason);
        w.raw("}");
    }
    w.raw("]");
    w.finish();
    return ESP_OK;
}

esp_err_t GpioApiService::handleConfigGet(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request, 503, ErrorCodes::Service::UNAVAILABLE, "GPIO service unavailable");
    }

    GPIO::GpioData config;
    if (!_service->getConfig(config)) {
        return Response::error(request, 503, "gpio/busy");
    }

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) {
        return ESP_FAIL;
    }
    w.raw("{");
    w.key(CONFIG::Keys::kChannels); w.raw("[");
    for (uint8_t i = 0; i < config.channelCount; i++) {
        if (i > 0) {
            w.raw(",");
        }
        if (!writeChannelConfig(w, config.channels[i])) {
            return ESP_FAIL;
        }
    }
    w.raw("]}");
    w.finish();
    return ESP_OK;
}

esp_err_t GpioApiService::handleConfigPost(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request, 503, ErrorCodes::Service::UNAVAILABLE, "GPIO service unavailable");
    }

    return parseJsonBody(
        request,
        kGpioPayloadBytes,
        [this, request](JsonDocument& jsonDocument) -> esp_err_t {
            if (!jsonDocument.is<JsonObject>()) {
                return Response::invalidJson(request);
            }

            GPIO::GpioData parsed;
            JsonObject root = jsonDocument.as<JsonObject>();
            if (!CONFIG::JSON::deserializeGpio(root, parsed)) {
                return Response::error(request, 400, "gpio/invalid_config");
            }

            if (!_service->updateConfig(parsed, true)) {
                return Response::error(request, 500, "gpio/update_failed");
            }

            return handleConfigGet(request);
        },
        kGpioPayloadBytes);
}

esp_err_t GpioApiService::handleStatus(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request, 503, ErrorCodes::Service::UNAVAILABLE, "GPIO service unavailable");
    }

    GPIO::GpioChannelStatus statuses[GPIO::kMaxChannels];
    uint8_t count = 0;
    if (!_service->getStatus(statuses, GPIO::kMaxChannels, count)) {
        return Response::error(request, 503, "gpio/busy");
    }

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) {
        return ESP_FAIL;
    }
    w.raw("{");
    w.key(CONFIG::Keys::kChannels); w.raw("[");
    for (uint8_t i = 0; i < count; i++) {
        if (i > 0) {
            w.raw(",");
        }
        if (!writeChannelStatus(w, statuses[i])) {
            return ESP_FAIL;
        }
    }
    w.raw("]}");
    w.finish();
    return ESP_OK;
}

esp_err_t GpioApiService::handleOutput(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request, 503, ErrorCodes::Service::UNAVAILABLE, "GPIO service unavailable");
    }

    return parseJsonBody(
        request,
        kGpioPayloadBytes,
        [this, request](JsonDocument& jsonDocument) -> esp_err_t {
            if (!jsonDocument.is<JsonObject>()) {
                return Response::invalidJson(request);
            }
            JsonObject root = jsonDocument.as<JsonObject>();
            const char* id = root[CONFIG::Keys::kId] | static_cast<const char*>(nullptr);
            if (!id || id[0] == '\0') {
                return Response::error(request, 400, ErrorCodes::Input::MISSING_FIELD, "Missing id");
            }
            if (!root[CONFIG::Keys::kValue].is<bool>()) {
                return Response::error(request, 400, ErrorCodes::Input::MISSING_FIELD, "Missing value");
            }

            const bool value = root[CONFIG::Keys::kValue].as<bool>();
            if (!_service->setOutput(id, value, true)) {
                return Response::error(request, 409, "gpio/output_rejected");
            }

            return Response::success(request, [id, value](JsonVariant& response) {
                response["ok"] = true;
                response[CONFIG::Keys::kId] = id;
                response[CONFIG::Keys::kValue] = value;
            });
        },
        kGpioPayloadBytes);
}

bool GpioApiService::writeChannelConfig(Utils::JsonResponseWriter& w,
                                        const GPIO::GpioChannelConfig& channel) {
    if (!w.raw("{")) return false;
    if (!w.key(CONFIG::Keys::kId) || !w.string(channel.id)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kName) || !w.string(channel.name)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kPin) || !w.value(static_cast<unsigned int>(channel.pin))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kMode) || !w.string(GPIO::modeToString(channel.mode))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kPull) || !w.string(GPIO::pullToString(channel.pull))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kInverted) || !w.value(channel.inverted)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kDebounceMs) || !w.value(static_cast<unsigned int>(channel.debounceMs))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kInitialOutput) || !w.value(channel.initialOutput)) return false;
    if (!w.raw("}")) return false;
    return true;
}

bool GpioApiService::writeChannelStatus(Utils::JsonResponseWriter& w,
                                        const GPIO::GpioChannelStatus& status) {
    if (!w.raw("{")) return false;
    if (!w.key(CONFIG::Keys::kId) || !w.string(status.config.id)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kName) || !w.string(status.config.name)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kPin) || !w.value(static_cast<unsigned int>(status.config.pin))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kMode) || !w.string(GPIO::modeToString(status.config.mode))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kConfigured) || !w.value(status.configured)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kRaw) || !w.value(status.rawLevel)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kLogical) || !w.value(status.logicalLevel)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kStable) || !w.value(status.stable)) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kSampledAt) || !w.value(static_cast<unsigned long>(status.sampledAtMs))) return false;
    if (!w.raw(",") || !w.key(CONFIG::Keys::kChangedAt) || !w.value(static_cast<unsigned long>(status.changedAtMs))) return false;
    if (!w.raw("}")) return false;
    return true;
}

}  // namespace API
