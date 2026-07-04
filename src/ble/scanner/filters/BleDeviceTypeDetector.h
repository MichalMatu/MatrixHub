#pragma once

#include <cstddef>
#include <cstdint>

namespace BLE {

/**
 * @brief Device type classification for BLE advertising packets.
 * 
 * Supports TP357 (ThermoPro) and BTHome (PVVX/ATC thermometers).
 */
enum class BleDeviceType {
    Unknown,
    TP357,    // ThermoPro TP357 via manufacturer data
    BTHome    // PVVX/ATC via BTHome service UUID 0xFCD2
};

/**
 * @brief Result of device type detection with optional BTHome data pointer.
 */
struct BleDeviceTypeResult {
    BleDeviceType type = BleDeviceType::Unknown;
    
    // BTHome-specific: pointer to service data within payload (no heap allocation)
    const uint8_t* btHomeData = nullptr;
    size_t btHomeDataLen = 0;
};

/**
 * @brief Detects BLE device type from advertising payload.
 * 
 * Zero-allocation detector: parses payload to identify TP357 or BTHome devices
 * without heap allocation. Returns pointer to BTHome service data if found.
 */
class BleDeviceTypeDetector {
public:
    /**
     * @brief Detect device type from advertising payload.
     * 
     * Detection strategy:
     * 1. Check name prefix (TP357, ATC, LYWSD)
     * 2. For BTHome candidates, locate BTHome service UUID 0xFCD2 data
     * 
     * @param payload BLE advertising payload (raw bytes)
     * @param payloadLen Payload length in bytes
     * @return Detection result with type and optional BTHome data pointer
     */
    static BleDeviceTypeResult detect(const uint8_t* payload, size_t payloadLen);
};

} // namespace BLE
