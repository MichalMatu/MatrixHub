/**
 * @file test_ble_device_cache.cpp
 * @brief Unit tests for BleDeviceCache throttling logic
 * 
 * Tests cover:
 * - Throttling based on time (10s cooldown)
 * - Throttling based on value change (0.15°C delta)
 * - Cache storage and retrieval
 * - Discovery cache LRU eviction
 */

#include <unity.h>
#include <cstring>
#include <cmath>
#include <cstdio>

#define NATIVE_BUILD 1

// ============================================================================
// Mock RTC Storage (simplified version for testing)
// ============================================================================

namespace BLE {
    constexpr size_t kMaxDeviceNameLen = 24;
    constexpr size_t kMaxMacAddressLen = 18;
    
    struct BleSensorConfig {
        char mac[kMaxMacAddressLen] = {0};
        char alias[kMaxDeviceNameLen] = {0};
        bool isEmpty() const { return mac[0] == '\0'; }
    };
}

namespace RTC {
    constexpr uint8_t kMaxBleSensors = 8;
    constexpr uint8_t kMaxBleDiscovered = 8;
    
    struct BleSensorReading {
        uint32_t lastSeenTime = 0;
        float temperature = -999.0f;
        float humidity = 0.0f;
        uint8_t battery = 0;
        int8_t rssi = 0;
    };
    
    struct BleDiscoveryEntry {
        char mac[18] = {0};
        uint32_t lastSeenTime = 0;
        float temperature = -999.0f;
        float humidity = 0.0f;
        uint8_t battery = 0;
        int8_t rssi = 0;
        
        bool isEmpty() const { return mac[0] == '\0'; }
        void clear() { mac[0] = '\0'; lastSeenTime = 0; temperature = -999.0f; }
    };
    
    struct BleData {
        bool enabled = true;
        bool advertisingEnabled = true;
        bool scannerEnabled = true;
        BLE::BleSensorConfig sensors[kMaxBleSensors];
        BleSensorReading readings[kMaxBleSensors];
        BleDiscoveryEntry discovered[kMaxBleDiscovered];
        uint8_t sensorCount = 0;
        uint8_t discoveredCount = 0;
    };
    
    struct ConfigStore {
        BleData ble;
    };
    
    // Global mock store
    ConfigStore store;
}

// ============================================================================
// Inline BleDeviceCache implementation for test
// ============================================================================

namespace BLE {

class BleDeviceCache {
public:
    static constexpr uint32_t kCooldownMs = 10000;
    static constexpr float kTempDeltaThreshold = 0.15f;
    
    bool shouldReport(const char* mac, float temperature, float humidity, uint8_t battery, int8_t rssi, uint32_t nowMs) {
        int idx = findOrAllocateSensorIndex(mac);
        if (idx < 0) {
            return true;
        }
        
        auto& reading = RTC::store.ble.readings[idx];
        
        bool timeExpired = (nowMs - reading.lastSeenTime > kCooldownMs);
        bool valueChanged = (std::abs(temperature - reading.temperature) > kTempDeltaThreshold);
        
        if (timeExpired || valueChanged) {
            reading.lastSeenTime = nowMs;
            reading.temperature = temperature;
            reading.humidity = humidity;
            reading.battery = battery;
            reading.rssi = rssi;
            return true;
        }
        
        return false;
    }
    
    void clear() {
        for (size_t i = 0; i < RTC::kMaxBleSensors; i++) {
            auto& reading = RTC::store.ble.readings[i];
            reading.lastSeenTime = 0;
            reading.temperature = -999.0f;
            reading.humidity = 0.0f;
            reading.battery = 0;
            reading.rssi = 0;
        }
    }
    
    bool getDeviceData(const char* mac, float& outTemp, float& outHumid, uint8_t& outBatt, int8_t& outRssi, uint32_t& outLastSeen) const {
        for (size_t i = 0; i < RTC::store.ble.sensorCount; i++) {
            if (strncmp(RTC::store.ble.sensors[i].mac, mac, 17) == 0) {
                const auto& reading = RTC::store.ble.readings[i];
                outTemp = reading.temperature;
                outHumid = reading.humidity;
                outBatt = reading.battery;
                outRssi = reading.rssi;
                outLastSeen = reading.lastSeenTime;
                return true;
            }
        }
        return false;
    }
    
    size_t getCachedDeviceCount() const {
        size_t count = 0;
        for (size_t i = 0; i < RTC::store.ble.sensorCount; i++) {
            if (RTC::store.ble.readings[i].lastSeenTime > 0) {
                count++;
            }
        }
        return count;
    }
    
    bool updateDiscoveryCache(const char* mac, float temp, float humid, uint8_t batt, int8_t rssi, uint32_t nowMs) {
        // Skip if in whitelist
        for (size_t i = 0; i < RTC::store.ble.sensorCount; i++) {
            if (strncmp(RTC::store.ble.sensors[i].mac, mac, 17) == 0) {
                return false;
            }
        }
        
        int idx = findOrAllocateDiscoverySlot(mac, nowMs);
        if (idx < 0) return false;
        
        auto& entry = RTC::store.ble.discovered[idx];
        strncpy(entry.mac, mac, sizeof(entry.mac) - 1);
        entry.temperature = temp;
        entry.humidity = humid;
        entry.battery = batt;
        entry.rssi = rssi;
        entry.lastSeenTime = nowMs;
        
        if (static_cast<size_t>(idx) >= RTC::store.ble.discoveredCount) {
            RTC::store.ble.discoveredCount = idx + 1;
        }
        return true;
    }
    
    void clearDiscoveryCache() {
        for (size_t i = 0; i < RTC::kMaxBleDiscovered; i++) {
            RTC::store.ble.discovered[i].clear();
        }
        RTC::store.ble.discoveredCount = 0;
    }

private:
    int findOrAllocateSensorIndex(const char* mac) {
        for (size_t i = 0; i < RTC::store.ble.sensorCount; i++) {
            if (strncmp(RTC::store.ble.sensors[i].mac, mac, 17) == 0) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    
    int findOrAllocateDiscoverySlot(const char* mac, uint32_t nowMs) {
        // Find existing
        for (size_t i = 0; i < RTC::kMaxBleDiscovered; i++) {
            if (!RTC::store.ble.discovered[i].isEmpty() && 
                strncmp(RTC::store.ble.discovered[i].mac, mac, 17) == 0) {
                return static_cast<int>(i);
            }
        }
        // Find empty
        for (size_t i = 0; i < RTC::kMaxBleDiscovered; i++) {
            if (RTC::store.ble.discovered[i].isEmpty()) {
                return static_cast<int>(i);
            }
        }
        // Evict oldest (LRU)
        size_t oldestIdx = 0;
        uint32_t oldestTime = RTC::store.ble.discovered[0].lastSeenTime;
        for (size_t i = 1; i < RTC::kMaxBleDiscovered; i++) {
            if (RTC::store.ble.discovered[i].lastSeenTime < oldestTime) {
                oldestTime = RTC::store.ble.discovered[i].lastSeenTime;
                oldestIdx = i;
            }
        }
        RTC::store.ble.discovered[oldestIdx].clear();
        return static_cast<int>(oldestIdx);
    }
};

}  // namespace BLE

using namespace BLE;

static BleDeviceCache cache;

// Helper to add device to whitelist
static void addToWhitelist(const char* mac, const char* alias = "") {
    size_t idx = RTC::store.ble.sensorCount;
    if (idx >= RTC::kMaxBleSensors) return;
    strncpy(RTC::store.ble.sensors[idx].mac, mac, 17);
    strncpy(RTC::store.ble.sensors[idx].alias, alias, 23);
    RTC::store.ble.sensorCount++;
}

void setUp(void) {
    // Reset RTC store
    memset(&RTC::store, 0, sizeof(RTC::store));
    cache.clear();
    cache.clearDiscoveryCache();
}

void tearDown(void) {}

// ============================================================================
// Test: Throttling - Time-based
// ============================================================================

void test_first_reading_always_reports() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    TEST_ASSERT_TRUE(result);
}

void test_immediate_second_reading_throttled() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    
    // Same value, 1 second later
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 2000);
    TEST_ASSERT_FALSE(result);
}

void test_reading_after_cooldown_reports() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    
    // 11 seconds later (> 10s cooldown)
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 12000);
    TEST_ASSERT_TRUE(result);
}

void test_reading_exactly_at_cooldown_threshold() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 0);
    
    // Exactly 10001ms (> 10000ms)
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 10001);
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// Test: Throttling - Value change
// ============================================================================

void test_significant_temp_change_reports_immediately() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    
    // Large delta (0.5 > 0.15 threshold), only 1 second later
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 25.5f, 50.0f, 80, -60, 2000);
    TEST_ASSERT_TRUE(result);
}

void test_small_temp_change_still_throttled() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    
    // Small delta (0.1 < 0.15 threshold)
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 25.1f, 50.0f, 80, -60, 2000);
    TEST_ASSERT_FALSE(result);
}

void test_negative_temp_change_reports() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    
    // Negative delta (-0.5)
    bool result = cache.shouldReport("AA:BB:CC:DD:EE:01", 24.5f, 50.0f, 80, -60, 2000);
    TEST_ASSERT_TRUE(result);
}

// ============================================================================
// Test: Cache storage
// ============================================================================

void test_cache_stores_values() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.5f, 55.0f, 85, -55, 5000);
    
    float temp, humid;
    uint8_t batt;
    int8_t rssi;
    uint32_t lastSeen;
    
    bool found = cache.getDeviceData("AA:BB:CC:DD:EE:01", temp, humid, batt, rssi, lastSeen);
    
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 25.5f, temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.0f, humid);
    TEST_ASSERT_EQUAL(85, batt);
    TEST_ASSERT_EQUAL(-55, rssi);
    TEST_ASSERT_EQUAL(5000, lastSeen);
}

void test_cache_count() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    addToWhitelist("AA:BB:CC:DD:EE:02");
    addToWhitelist("AA:BB:CC:DD:EE:03");
    
    TEST_ASSERT_EQUAL(0, cache.getCachedDeviceCount());
    
    cache.shouldReport("AA:BB:CC:DD:EE:01", 25.0f, 50.0f, 80, -60, 1000);
    TEST_ASSERT_EQUAL(1, cache.getCachedDeviceCount());
    
    cache.shouldReport("AA:BB:CC:DD:EE:02", 26.0f, 55.0f, 90, -50, 2000);
    TEST_ASSERT_EQUAL(2, cache.getCachedDeviceCount());
}

void test_non_whitelisted_device_not_cached() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    // Report for device NOT in whitelist
    bool result = cache.shouldReport("FF:FF:FF:FF:FF:FF", 25.0f, 50.0f, 80, -60, 1000);
    
    // Should report (not throttled) but not stored
    TEST_ASSERT_TRUE(result);
    
    float temp, humid;
    uint8_t batt;
    int8_t rssi;
    uint32_t lastSeen;
    
    bool found = cache.getDeviceData("FF:FF:FF:FF:FF:FF", temp, humid, batt, rssi, lastSeen);
    TEST_ASSERT_FALSE(found);
}

// ============================================================================
// Test: Discovery cache
// ============================================================================

void test_discovery_adds_new_device() {
    bool result = cache.updateDiscoveryCache("DD:DD:DD:DD:DD:01", 22.0f, 45.0f, 70, -70, 1000);
    TEST_ASSERT_TRUE(result);
}

void test_discovery_skips_whitelisted() {
    addToWhitelist("AA:BB:CC:DD:EE:01");
    
    bool result = cache.updateDiscoveryCache("AA:BB:CC:DD:EE:01", 22.0f, 45.0f, 70, -70, 1000);
    TEST_ASSERT_FALSE(result);
}

void test_discovery_lru_eviction() {
    // Fill all discovery slots
    for (int i = 0; i < RTC::kMaxBleDiscovered; i++) {
        char mac[18];
        snprintf(mac, sizeof(mac), "DD:DD:DD:DD:DD:%02X", i);
        cache.updateDiscoveryCache(mac, 20.0f + i, 50.0f, 80, -60, 1000 + i * 100);
    }
    
    // Add one more - should evict oldest (slot 0, timestamp 1000)
    cache.updateDiscoveryCache("EE:EE:EE:EE:EE:FF", 30.0f, 60.0f, 90, -50, 5000);
    
    // Slot 0 should now have the new device
    TEST_ASSERT_EQUAL_STRING("EE:EE:EE:EE:EE:FF", RTC::store.ble.discovered[0].mac);
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    // Time-based throttling
    RUN_TEST(test_first_reading_always_reports);
    RUN_TEST(test_immediate_second_reading_throttled);
    RUN_TEST(test_reading_after_cooldown_reports);
    RUN_TEST(test_reading_exactly_at_cooldown_threshold);
    
    // Value-based throttling
    RUN_TEST(test_significant_temp_change_reports_immediately);
    RUN_TEST(test_small_temp_change_still_throttled);
    RUN_TEST(test_negative_temp_change_reports);
    
    // Cache storage
    RUN_TEST(test_cache_stores_values);
    RUN_TEST(test_cache_count);
    RUN_TEST(test_non_whitelisted_device_not_cached);
    
    // Discovery cache
    RUN_TEST(test_discovery_adds_new_device);
    RUN_TEST(test_discovery_skips_whitelisted);
    RUN_TEST(test_discovery_lru_eviction);
    
    return UNITY_END();
}
