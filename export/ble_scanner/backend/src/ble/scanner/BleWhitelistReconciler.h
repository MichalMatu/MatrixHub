#pragma once

#include "../../system/rtc/types/RtcBleTypes.h"
#include "../BleTypes.h"

#include <cctype>
#include <cstring>

namespace BLE::detail {

inline RTC::BleSensorReading makeInvalidReading() {
    RTC::BleSensorReading reading{};
    reading.lastSeenTime = 0;
    reading.temperature = -999.0f;
    reading.humidity = 0.0f;
    reading.battery = 0;
    reading.rssi = 0;
    memset(reading._pad, 0, sizeof(reading._pad));
    return reading;
}

inline bool hasValidReading(const RTC::BleSensorReading& reading) {
    return reading.lastSeenTime != 0 && reading.temperature > -100.0f;
}

inline bool hasValidDiscoveryEntry(const RTC::BleDiscoveryEntry& entry) {
    return !entry.isEmpty() && entry.lastSeenTime != 0 && entry.temperature > -100.0f;
}

inline bool macEquals(const char* lhs, const char* rhs) {
    if (!lhs || !rhs) {
        return false;
    }

    for (size_t i = 0; i < BLE::kMaxMacAddressLen; i++) {
        const unsigned char lhsCh = static_cast<unsigned char>(lhs[i]);
        const unsigned char rhsCh = static_cast<unsigned char>(rhs[i]);
        const char lhsNorm = static_cast<char>(std::tolower(lhsCh));
        const char rhsNorm = static_cast<char>(std::tolower(rhsCh));

        if (lhsNorm != rhsNorm) {
            return false;
        }

        if (lhsNorm == '\0') {
            return true;
        }
    }

    return true;
}

inline bool tryCopyReadingByMac(const RTC::BleData& previousState,
                                const char* mac,
                                RTC::BleSensorReading& outReading) {
    if (!mac || mac[0] == '\0') {
        return false;
    }

    for (size_t i = 0; i < previousState.sensorCount && i < RTC::kMaxBleSensors; i++) {
        if (!macEquals(previousState.sensors[i].mac, mac)) {
            continue;
        }

        if (!hasValidReading(previousState.readings[i])) {
            return false;
        }

        outReading = previousState.readings[i];
        return true;
    }

    return false;
}

inline bool tryCopyReadingFromDiscovery(const RTC::BleDiscoveryEntry* discovered,
                                        size_t discoveredCount,
                                        const char* mac,
                                        RTC::BleSensorReading& outReading) {
    if (!discovered || !mac || mac[0] == '\0') {
        return false;
    }

    const size_t boundedCount =
        (discoveredCount < RTC::kMaxBleDiscovered) ? discoveredCount : RTC::kMaxBleDiscovered;

    for (size_t i = 0; i < boundedCount; i++) {
        const auto& entry = discovered[i];
        if (!macEquals(entry.mac, mac) || !hasValidDiscoveryEntry(entry)) {
            continue;
        }

        outReading = makeInvalidReading();
        outReading.lastSeenTime = entry.lastSeenTime;
        outReading.temperature = entry.temperature;
        outReading.humidity = entry.humidity;
        outReading.battery = entry.battery;
        outReading.rssi = entry.rssi;
        return true;
    }

    return false;
}

inline void reconcileWhitelistState(RTC::BleData& state,
                                    const RTC::BleDiscoveryEntry* discovered,
                                    size_t discoveredCount,
                                    const BleSensorConfig* sensors,
                                    size_t count) {
    const RTC::BleData previousState = state;
    const size_t nextCount =
        (sensors != nullptr)
            ? ((count < RTC::kMaxBleSensors) ? count : RTC::kMaxBleSensors)
            : 0;

    state.sensorCount = static_cast<uint8_t>(nextCount);

    for (size_t i = 0; i < RTC::kMaxBleSensors; i++) {
        state.sensors[i] = BleSensorConfig{};
        state.readings[i] = makeInvalidReading();
    }

    for (size_t i = 0; i < nextCount; i++) {
        state.sensors[i] = sensors[i];

        RTC::BleSensorReading nextReading = makeInvalidReading();
        if (!tryCopyReadingByMac(previousState, state.sensors[i].mac, nextReading)) {
            tryCopyReadingFromDiscovery(discovered, discoveredCount, state.sensors[i].mac, nextReading);
        }

        state.readings[i] = nextReading;
    }
}

}  // namespace BLE::detail
