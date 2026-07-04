/**
 * @file SensorsCommand.cpp
 * @brief Implementation of /sensors command
 */

#include "SensorsCommand.h"
#include "TelegramCommandSections.h"
#include "TelegramReplyBuilder.h"

#include <cstdio>

#include "../../../sensors/runtime/SensorState.h"
#include "../../../system/rtc/RtcConfig.h"

namespace TELEGRAM::Commands {

bool handleSensors(CommandContext& ctx) {
    // Get latest sensor readings
    SensorSnapshot snap = SENSORS::SensorState::getLastGoodSnapshot();
    PhaseStatus status = SENSORS::SensorState::getLastReadStatus();

    // Gather RTC configurations safely for external devices
    uint8_t activeShelly = 0;
    uint8_t activeBle = 0;

    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        activeShelly = cfg.shelly.enabledCount;
        // Count active BLE sensors
        activeBle = cfg.ble.sensorCount;
    });

    TelegramReplyBuilder reply(ctx);
    reply.header("🌡️", "Sensor Readings");

    if (!status.ok || snap.timestamp_ms == 0) {
        reply.section("🏠", "Ambient");
        reply.line("⚠️ No valid readings available.");
        reply.detailf("Status: %s", status.error_code ? status.error_code : "unknown");
        reply.line();
        reply.section("🔌", "External Devices");
        reply.kvf("⚡", "Shelly relays", "%u", activeShelly);
        reply.kvf("📡", "BLE sensors", "%u", activeBle);
        if (activeBle > 0) {
            reply.line();
            Sections::appendBleSection(reply, false, false);
        }
    } else {
        // Calculate age of reading
        uint32_t ageSec = (millis() - snap.timestamp_ms) / 1000;

        reply.section("🏠", "Ambient");
        reply.kvf("🌡️", "Temp", "%.1f C", snap.temp);
        reply.kvf("💧", "Humid", "%.1f%%", snap.humid);
        reply.kvf("💨", "CO2", "%u ppm", snap.co2);
        reply.line();
        reply.section("🔌", "External Devices");
        reply.kvf("⚡", "Shelly relays", "%u", activeShelly);
        reply.kvf("📡", "BLE sensors", "%u", activeBle);
        if (activeBle > 0) {
            reply.line();
            Sections::appendBleSection(reply, false, false);
        }
        reply.line();
        reply.section("🕒", "Snapshot");
        reply.kvf("📍", "Reading", "#%u (%us ago)", snap.seq, ageSec);
    }

    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
