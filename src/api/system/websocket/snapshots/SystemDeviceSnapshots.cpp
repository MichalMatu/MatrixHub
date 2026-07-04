#include "SystemSnapshots.h"

#include "../../../../ble/BleLastSeenTime.h"
#include "../../../../ble/BleService.h"
#include "../../../../config/System.h"
#include "../../../../shelly/ShellyService.h"
#include "../../../../system/rtc/RtcConfig.h"
#include "../../../../wifisensing/WifiSensingService.h"

#include <ArduinoJson.h>

#include <time.h>

namespace API::SYSTEM_WS {

void sendShellySnapshot(const SnapshotContext& ctx) {
    if (!ctx.shellyService) {
        return;
    }

    SYSTEM::SpiRamJsonDocument doc(8192);
    doc["type"] = "snapshot";
    doc["channel"] = "shelly";
    JsonArray data = doc["data"].to<JsonArray>();

    using namespace CONFIG::Keys;
    ctx.shellyService->forAll([&](const SHELLY::ShellyDevice& device) {
        JsonObject obj = data.add<JsonObject>();
        obj[kId] = device.id;
        obj[kName] = device.name;
        obj[kIp] = device.ip;
        obj[kRelayIndex] = device.relayIndex;
        obj[kEnabled] = device.enabled;
        obj[kGeneration] = device.generation;
        obj[kIsOn] = device.isOn;
        obj[kIsOnline] = device.isOnline;
        obj[kLastUpdate] = static_cast<unsigned long>(device.lastUpdate);
        obj[kPower] = device.power;
        obj[kEnergy] = device.energy;
        obj[kVoltage] = device.voltage;
        obj[kCurrent] = device.current;
        obj[kTemp] = device.temperature;
        obj[kRssi] = device.rssi;
    });

    sendSnapshotDoc(ctx.ws, ctx.fd, doc);
}

void sendSensingSnapshot(const SnapshotContext& ctx) {
    RTC::WifiSensingData cfg{};
    RTC::withConfig([&](const RTC::ConfigStore& store) {
        cfg = store.wifiSensing;
    });

    SYSTEM::SpiRamJsonDocument doc(1024);
    doc["type"] = "snapshot";
    doc["channel"] = "sensing";
    JsonObject data = doc["data"].to<JsonObject>();
    data[CONFIG::Keys::kEnabled] = cfg.enabled;
    data[CONFIG::Keys::kSampleIntervalMs] = cfg.sampleIntervalMs;
    data[CONFIG::Keys::kVarianceThreshold] = cfg.varianceThreshold;

    sendSnapshotDoc(ctx.ws, ctx.fd, doc);
}

void sendBleSnapshot(const SnapshotContext& ctx) {
    if (!ctx.bleService) {
        return;
    }

    RTC::BleData cfg = RTC::copyConfigSection(&RTC::ConfigStore::ble);
    const auto status = ctx.bleService->getStatus();
    SYSTEM::SpiRamJsonDocument doc(8192);
    doc["type"] = "snapshot";
    doc["channel"] = "ble";
    JsonObject data = doc["data"].to<JsonObject>();

    using namespace CONFIG::Keys;
    data[kEnabled] = cfg.enabled;
    data[kRunning] = ctx.bleService->isRunning();
    data[kScannerActive] = status.scannerActive;

    JsonObject settings = data["settings"].to<JsonObject>();
    settings[kEnabled] = cfg.enabled;

    JsonArray sensors = settings[kSensors].to<JsonArray>();
    for (uint8_t i = 0; i < cfg.sensorCount && i < RTC::kMaxBleSensors; i++) {
        const auto& sensor = cfg.sensors[i];
        if (sensor.isEmpty()) {
            continue;
        }

        JsonObject obj = sensors.add<JsonObject>();
        obj[kMac] = sensor.mac;
        obj[kAlias] = sensor.alias;
    }

    JsonArray devices = data["devices"].to<JsonArray>();
    const uint32_t nowMs = millis();
    const time_t wallClockNow = time(nullptr);
    const uint64_t nowEpochMs = static_cast<uint64_t>(wallClockNow) * 1000ULL;
    // Keep snapshot semantics aligned with /api/ble/status so both transports
    // either expose a trustworthy epoch timestamp or the same 0 sentinel.
    const bool wallClockValid = BLE::LastSeenTime::isWallClockValid(wallClockNow);

    auto addDevice = [&](const char* mac,
                         float temp,
                         float humid,
                         uint8_t batt,
                         int8_t rssi,
                         uint32_t lastSeenMs,
                         bool saved) {
        if (!mac || mac[0] == '\0') {
            return;
        }

        JsonObject obj = devices.add<JsonObject>();
        obj[kMac] = mac;
        obj[kTemp] = temp;
        obj[kHumid] = humid;
        obj[kBatt] = static_cast<int>(batt);
        obj[kRssi] = static_cast<int>(rssi);

        // Mirror the REST BLE contract: last_seen=0 means "reading age is known
        // only relatively via millis(), but wall-clock time is not trustworthy yet".
        const uint64_t lastSeenEpochMs = BLE::LastSeenTime::toEpochMsOrZero(
            nowMs, lastSeenMs, nowEpochMs, wallClockValid);
        obj[kLastSeen] = static_cast<unsigned long long>(lastSeenEpochMs);
        obj[kSaved] = saved;
    };

    for (size_t slot = 0; slot < ctx.bleService->getCachedDeviceSlots(); slot++) {
        const char* mac = nullptr;
        float temp = 0.0f;
        float humid = 0.0f;
        uint8_t batt = 0;
        int8_t rssi = 0;
        uint32_t lastSeenMs = 0;
        if (ctx.bleService->getCachedDeviceDataAt(slot, mac, temp, humid, batt, rssi, lastSeenMs)) {
            addDevice(mac, temp, humid, batt, rssi, lastSeenMs, true);
        }
    }

    sendSnapshotDoc(ctx.ws, ctx.fd, doc);
}

}  // namespace API::SYSTEM_WS
