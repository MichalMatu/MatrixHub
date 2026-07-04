#pragma once

#include <Arduino.h>
#include <cstring>

namespace RTC {

inline uint32_t calculateSsidCrc(const char* ssid) {
    uint32_t crc = 2166136261u;  // FNV-1a 32-bit offset basis
    if (!ssid) {
        return 0;
    }

    while (*ssid) {
        crc ^= static_cast<uint8_t>(*ssid++);
        crc *= 16777619u;  // FNV prime
    }

    return crc;
}

/**
 * WiFi connection cache for fast reconnect after deep sleep.
 * Stores negotiated parameters to skip scanning phase.
 * 
 * Usage:
 * - After successful WiFi connection: call cacheCurrentState()
 * - On warm boot: use cached BSSID/channel for fast reconnect
 * 
 * Size: 24 bytes (aligned)
 */
struct RtcNetworkState {
    uint32_t magic = 0;           // Validation (0x57494649 = "WIFI")
    uint8_t channel = 0;          // Last connected channel (1-14)
    uint8_t bssid[6] = {0};       // Last connected AP MAC
    uint8_t _pad1 = 0;            // Alignment
    uint32_t localIp = 0;         // Static IP (network byte order)
    uint32_t gateway = 0;         // Gateway IP
    uint32_t netmask = 0;         // Subnet mask
    uint32_t ssidCrc = 0;         // SSID fingerprint for cache matching
    bool valid = false;           // Data validity flag
    uint8_t _pad2[3] = {0};       // Alignment to 32 bytes
    
    /** Check if cached data is valid for fast-connect */
    bool isValid() const {
        return magic == 0x57494649 && valid && channel > 0 && channel <= 14;
    }

    /** Check whether cache belongs to the requested SSID */
    bool matchesSsid(uint32_t expectedSsidCrc) const {
        return isValid() && ssidCrc == expectedSsidCrc;
    }
    
    /** Invalidate cache (forces full scan on next connect) */
    void invalidate() {
        valid = false;
        magic = 0;
        channel = 0;
        std::memset(bssid, 0, sizeof(bssid));
        localIp = 0;
        gateway = 0;
        netmask = 0;
        ssidCrc = 0;
    }
    
    /** Mark cache as valid */
    void markValid() {
        magic = 0x57494649;  // "WIFI"
        valid = true;
    }

    /** Store a full negotiated connection snapshot for fast reconnect */
    void update(uint8_t newChannel,
                const uint8_t* newBssid,
                uint32_t newLocalIp,
                uint32_t newGateway,
                uint32_t newNetmask,
                uint32_t newSsidCrc) {
        if (!newBssid || newChannel == 0 || newChannel > 14 || newSsidCrc == 0) {
            invalidate();
            return;
        }

        channel = newChannel;
        std::memcpy(bssid, newBssid, sizeof(bssid));
        localIp = newLocalIp;
        gateway = newGateway;
        netmask = newNetmask;
        ssidCrc = newSsidCrc;
        markValid();
    }
};

constexpr uint32_t kNetworkStateMagic = 0x57494649;  // "WIFI"

// Compile-time size check
static_assert(sizeof(RtcNetworkState) <= 32, "RtcNetworkState exceeds expected size");

}  // namespace RTC
