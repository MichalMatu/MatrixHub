#pragma once

#include "../../../sensors/model/SensorTypes.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace API {

constexpr uint8_t kSensorTelemetryMagic = 0x54;  // 'T'
constexpr size_t kSensorTelemetryPacketSize = 12;

// Keep the tiny binary packet encoding in one helper so broadcaster code,
// tests and future decoder changes all share one authoritative wire layout.
// Byte 11 is intentionally a flags byte instead of a bare bool so we can grow
// the packet with more sensor-health bits later without reshaping the payload.
inline size_t encodeSensorTelemetryPacket(const SensorSnapshot& snap,
                                          bool lastReadOk,
                                          uint8_t* outBuffer,
                                          size_t bufferSize) {
    if (!outBuffer || bufferSize < kSensorTelemetryPacketSize) {
        return 0;
    }

    size_t off = 0;
    outBuffer[off++] = kSensorTelemetryMagic;

    const uint16_t co2 = static_cast<uint16_t>(snap.co2);
    memcpy(&outBuffer[off], &co2, sizeof(co2));
    off += sizeof(co2);

    const int16_t temp = static_cast<int16_t>(snap.temp * 10.0f);
    memcpy(&outBuffer[off], &temp, sizeof(temp));
    off += sizeof(temp);

    const uint16_t humid = static_cast<uint16_t>(snap.humid * 10.0f);
    memcpy(&outBuffer[off], &humid, sizeof(humid));
    off += sizeof(humid);

    const uint32_t ts = static_cast<uint32_t>(snap.timestamp_ms);
    memcpy(&outBuffer[off], &ts, sizeof(ts));
    off += sizeof(ts);

    outBuffer[off++] = lastReadOk ? 0x01 : 0x00;
    return off;
}

}  // namespace API
