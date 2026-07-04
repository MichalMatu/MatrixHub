/**
 * @file ShellyApiService.cpp
 * @brief Implementation of Shelly API endpoint router (facade)
 * 
 * Endpoints:
 * GET /api/shelly/devices - List configured devices
 * POST /api/shelly/devices - Add or update device
 * DELETE /api/shelly/devices?id=... - Remove device
 * POST /api/shelly/control - Control relay state
 */

#include "ShellyApiService.h"
#include <utils/ResponseUtils.h>
#include "../../config/System.h"
#include "../../system/utils/json/JsonResponseWriter.h"
#include <ArduinoJson.h>

namespace API {

ShellyApiService::ShellyApiService(PsychicHttpServer* server, SecurityManager* security, POWER::PowerManager* powerManager, SHELLY::ShellyService* service)
    : BaseApiService(server, security, powerManager, "api/shelly"), _service(service) {
}

void ShellyApiService::begin() {
    _server->on(
        "/api/shelly/devices",
        HTTP_GET,
        wrapAuth([this](PsychicRequest* request) { 
            return this->handleDeviceList(request); 
        })
    );

    _server->on(
        "/api/shelly/devices",
        HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) { 
            return this->handleDeviceUpsert(request); 
        })
    );

    _server->on(
        "/api/shelly/devices",
        HTTP_DELETE,
        wrapAdmin([this](PsychicRequest* request) { 
            return this->handleDeviceDelete(request); 
        })
    );

    _server->on(
        "/api/shelly/control",
        HTTP_POST,
        wrapAdmin([this](PsychicRequest* request) { 
            return this->handleRelayControl(request); 
        })
    );
}

esp_err_t ShellyApiService::handleDeviceList(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request, 500, "internal/service_unavailable");
    }

    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) {
        return ESP_FAIL;
    }

    w.raw("[");
    bool first = true;

    using namespace CONFIG::Keys;
    _service->forAll([&](const SHELLY::ShellyDevice& d) {
        if (!first) {
            w.raw(",");
        }
        first = false;

        w.raw("{");
        w.key(kId); w.string(d.id);
        w.raw(","); w.key(kName); w.string(d.name);
        w.raw(","); w.key(kIp); w.string(d.ip);
        w.raw(","); w.key(kRelayIndex); w.value(static_cast<unsigned int>(d.relayIndex));
        w.raw(","); w.key(kEnabled); w.value(d.enabled);
        w.raw(","); w.key(kGeneration); w.value(static_cast<unsigned int>(d.generation));
        w.raw(","); w.key(kIsOn); w.value(d.isOn);
        w.raw(","); w.key(kIsOnline); w.value(d.isOnline);
        w.raw(","); w.key(kLastUpdate); w.value(static_cast<unsigned long>(d.lastUpdate));
        w.raw(","); w.key(kPower); w.value(d.power, 1);
        w.raw(","); w.key(kEnergy); w.value(d.energy, 1);
        w.raw(","); w.key(kVoltage); w.value(d.voltage, 1);
        w.raw(","); w.key(kCurrent); w.value(d.current, 3);
        w.raw(","); w.key(kTemp); w.value(d.temperature, 1);
        w.raw(","); w.key(kRssi); w.value(static_cast<int>(d.rssi));
        w.raw("}");
    });

    w.raw("]");
    w.finish();
    return ESP_OK;
}

esp_err_t ShellyApiService::handleDeviceUpsert(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request,
                               503,
                               ErrorCodes::Service::UNAVAILABLE,
                               "Shelly service unavailable");
    }

    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::SHELLY_DEVICE_UPSERT);
    if (auto err = Response::parseJsonBody(
            request, doc, LIMITS::API::JSON_DOC::SHELLY_DEVICE_UPSERT); err != ESP_OK) {
        return err;
    }

    const char* id = doc[CONFIG::Keys::kId] | "";
    const char* ip = doc[CONFIG::Keys::kIp] | "";
    const char* name = doc[CONFIG::Keys::kName] | "Shelly";

    if (!id || !id[0] || !ip || !ip[0]) {
        return Response::error(request, 400, ErrorCodes::Input::MISSING_FIELD, [](JsonVariant& root) {
            root["field"] = "id/ip";
            root["message"] = "Missing id or ip";
        });
    }
    
    if (!SHELLY::ShellyService::isValidPrivateIp(ip)) {
        return Response::error(request, 400, ErrorCodes::Input::INVALID_FORMAT, [](JsonVariant& root) {
            root["field"] = CONFIG::Keys::kIp;
            root["message"] = "Invalid IP address. Only private network IPs are allowed (192.168.x.x, 10.x.x.x, 172.16-31.x.x)";
        });
    }

    SHELLY::ShellyDevice device;
    strlcpy(device.id, id, sizeof(device.id));
    strlcpy(device.name, name, sizeof(device.name));
    strlcpy(device.ip, ip, sizeof(device.ip));
    int relayIndex = doc[CONFIG::Keys::kRelayIndex] | 0;
    if (relayIndex < 0 || relayIndex > 3) relayIndex = 0;
    device.relayIndex = static_cast<uint8_t>(relayIndex);

    uint8_t gen = doc[CONFIG::Keys::kGeneration] | 2;
    if (gen < 1 || gen > 2) gen = 2;
    device.generation = gen;
    
    device.enabled = doc[CONFIG::Keys::kEnabled] | true;

    // ShellyService now owns post-save lifecycle decisions (lazy worker start).
    // Keeping the API transport-only makes later debugging simpler because the
    // start/stop rules live in one backend layer instead of being split here.
    if (_service->upsertDevice(device)) {
        return Response::success(request, [](JsonVariant& root) {
            root["status"] = "ok";
        });
    } else {
        return Response::error(request, 500, ErrorCodes::Internal::UNEXPECTED_ERROR, "Failed to save");
    }
}

esp_err_t ShellyApiService::handleDeviceDelete(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request,
                               503,
                               ErrorCodes::Service::UNAVAILABLE,
                               "Shelly service unavailable");
    }

    if (!request->hasParam("id")) {
        return Response::error(request, 400, ErrorCodes::Input::MISSING_FIELD, [](JsonVariant& root) {
            root["field"] = CONFIG::Keys::kId;
            root["message"] = "Missing id";
        });
    }
    
    const char* id = request->getParam("id")->value().c_str();

    // ShellyService also owns the "last device removed => stop worker" rule.
    // This handler only translates HTTP to service calls and no longer carries
    // hidden lifecycle side effects that could be missed during debugging.
    if (_service->removeDevice(id)) {
        return Response::success(request, [](JsonVariant& root) {
            root["ok"] = true;
        });
    } else {
        return Response::error(request, 404, ErrorCodes::Shelly::DEVICE_NOT_FOUND, "Shelly device not found");
    }
}

esp_err_t ShellyApiService::handleRelayControl(PsychicRequest* request) {
    if (!_service) {
        return Response::error(request,
                               503,
                               ErrorCodes::Service::UNAVAILABLE,
                               "Shelly service unavailable");
    }

    SYSTEM::SpiRamJsonDocument doc(LIMITS::API::JSON_DOC::SHELLY_RELAY_CONTROL);
    if (auto err = Response::parseJsonBody(
            request, doc, LIMITS::API::JSON_DOC::SHELLY_RELAY_CONTROL); err != ESP_OK) {
        return err;
    }

    const char* id = doc[CONFIG::Keys::kId] | "";

    if (!id || id[0] == '\0') {
        return Response::error(request, 400, ErrorCodes::Input::MISSING_FIELD, [](JsonVariant& root) {
            root["field"] = CONFIG::Keys::kId;
            root["message"] = "Missing id";
        });
    }

    if (!doc["on"].is<bool>()) {
        return Response::error(request, 400, ErrorCodes::Input::MISSING_FIELD, [](JsonVariant& root) {
            root["field"] = "on";
            root["message"] = "Missing on";
        });
    }

    SHELLY::ShellyDevice device;
    if (!_service->getDevice(id, device)) {
        return Response::error(request, 404, ErrorCodes::Shelly::DEVICE_NOT_FOUND, "Shelly device not found");
    }

    if (!device.enabled) {
        return Response::error(request, 409, ErrorCodes::Shelly::DEVICE_DISABLED, "Shelly device is disabled");
    }

    const bool turnOn = doc["on"].as<bool>();
    if (!_service->setRelayState(id, turnOn)) {
        return Response::error(request,
                               503,
                               ErrorCodes::Shelly::CONTROL_NOT_QUEUED,
                               "Shelly relay command was not queued");
    }

    return Response::success(request, [id, turnOn](JsonVariant& root) {
        root["ok"] = true;
        root["status"] = "queued";
        root[CONFIG::Keys::kId] = id;
        root["on"] = turnOn;
    });
}

} // namespace API
