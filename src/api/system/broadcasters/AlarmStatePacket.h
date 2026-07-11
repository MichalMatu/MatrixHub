#pragma once

#include "../../../alarms/core/AlarmCoordinator.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace API {

constexpr uint8_t kAlarmStateMagic = 0x41;  // 'A'
constexpr size_t kAlarmStateLegacyPacketSize =
    1 + ALARMS::kMaxIdLen + 1 + 1 + sizeof(float);
constexpr size_t kAlarmStatePacketSize =
    kAlarmStateLegacyPacketSize + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t);

inline void writeAlarmPacketUint32Le(uint8_t* out, uint32_t value) {
    out[0] = static_cast<uint8_t>(value & 0xFFU);
    out[1] = static_cast<uint8_t>((value >> 8U) & 0xFFU);
    out[2] = static_cast<uint8_t>((value >> 16U) & 0xFFU);
    out[3] = static_cast<uint8_t>((value >> 24U) & 0xFFU);
}

// The first 39 bytes are the legacy alarm packet byte-for-byte. New metadata is
// append-only so older clients can keep decoding the stable prefix, while newer
// clients detect the optional suffix by packet length.
inline size_t encodeAlarmStatePacket(const ALARMS::AlarmStateChange& change,
                                     uint8_t* outBuffer,
                                     size_t bufferSize) {
    if (!outBuffer || bufferSize < kAlarmStatePacketSize) {
        return 0;
    }

    memset(outBuffer, 0, kAlarmStatePacketSize);
    size_t offset = 0;

    outBuffer[offset++] = kAlarmStateMagic;
    memcpy(&outBuffer[offset], change.id, ALARMS::kMaxIdLen);
    offset += ALARMS::kMaxIdLen;
    outBuffer[offset++] = change.triggered ? 1U : 0U;
    outBuffer[offset++] = static_cast<uint8_t>(change.severity);
    memcpy(&outBuffer[offset], &change.currentValue, sizeof(change.currentValue));
    offset += sizeof(change.currentValue);

    writeAlarmPacketUint32Le(&outBuffer[offset], change.transitionSeq);
    offset += sizeof(change.transitionSeq);
    writeAlarmPacketUint32Le(&outBuffer[offset], change.deviceMillis);
    offset += sizeof(change.deviceMillis);
    writeAlarmPacketUint32Le(&outBuffer[offset], static_cast<uint32_t>(change.bootId));
    offset += sizeof(uint32_t);
    writeAlarmPacketUint32Le(
        &outBuffer[offset], static_cast<uint32_t>(change.bootId >> 32U));
    offset += sizeof(uint32_t);

    return offset;
}

}  // namespace API
