/**
 * @file BleDeviceWhitelist.h
 * @brief Thread-safe whitelist management for BLE devices
 * 
 * Extracted from BleScanner to separate whitelist logic from scanning.
 */

#pragma once

#include <atomic>
#include <cstddef>
#include "../../BleTypes.h"
#include "../../../system/rtc/RtcConfig.h"

namespace BLE {

/**
 * Thread-safe whitelist for BLE device MAC addresses
 * 
 * Responsibilities:
 * - Store whitelist of BleSensorConfig (MAC + name)
 * - Atomic count tracking
 * - Spinlock-like protection for updates
 * - Fast MAC lookup
 * 
 * Does NOT handle:
 * - Scanning logic (see BleScanner)
 * - Data caching (see BleDeviceCache)
 */
class BleDeviceWhitelist {
public:
    BleDeviceWhitelist();
    
    /**
     * Update whitelist with new sensor list
     * Thread-safe with spinlock protection.
     * @param sensors Array of sensor configs
     * @param count Number of sensors (capped to kMaxBleSensors)
     */
    void update(const BleSensorConfig* sensors, size_t count);
    
    /**
     * Check if MAC address is in whitelist
     * Lock-free, returns false during update.
     * @param mac MAC address string (format XX:XX:XX:XX:XX:XX)
     * @return true if whitelisted
     */
    bool isWhitelisted(const char* mac) const;
    
    /**
     * Get current whitelist count
     */
    size_t count() const { return _count.load(std::memory_order_relaxed); }
    
    /**
     * Check if whitelist is empty
     */
    bool isEmpty() const { return count() == 0; }
    
    /**
     * Check if update is in progress
     */
    bool isUpdating() const { return _updating.load(std::memory_order_relaxed); }

    ~BleDeviceWhitelist();

private:
    BleSensorConfig* _whitelist = nullptr;
    std::atomic<size_t> _count{0};
    std::atomic<bool> _updating{false}; // Spinlock-like flag
};

}  // namespace BLE
