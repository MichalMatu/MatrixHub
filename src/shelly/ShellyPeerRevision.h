#pragma once

#include <cstddef>
#include <cstdint>

#include "ShellyTypes.h"

namespace SHELLY {

/**
 * Stable identity of the peer configuration used by a relay command.
 *
 * Runtime telemetry and presentation-only fields deliberately do not
 * participate. A command must be retried when its network target, relay,
 * protocol generation, or enabled state changes while transport is in flight.
 */
inline uint64_t shellyPeerRevision(const ShellyDevice& device) {
    constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
    constexpr uint64_t kFnvPrime = 1099511628211ULL;

    uint64_t hash = kFnvOffset;
    const auto append = [&hash](uint8_t byte) {
        hash ^= byte;
        hash *= kFnvPrime;
    };

    for (size_t i = 0; i < sizeof(device.id); ++i) {
        const uint8_t byte = static_cast<uint8_t>(device.id[i]);
        append(byte);
        if (byte == 0) {
            break;
        }
    }
    append(0xff);
    for (size_t i = 0; i < sizeof(device.ip); ++i) {
        const uint8_t byte = static_cast<uint8_t>(device.ip[i]);
        append(byte);
        if (byte == 0) {
            break;
        }
    }
    append(device.relayIndex);
    append(device.generation);
    append(device.enabled ? 1 : 0);
    return hash;
}

inline bool shellyPeerMatches(const ShellyDevice& current,
                              const ShellyDevice& expected) {
    return shellyPeerRevision(current) == shellyPeerRevision(expected);
}

}  // namespace SHELLY
