/**
 * @file test_ble_whitelist_reconciler.cpp
 * @brief Unit tests for BLE whitelist/runtime reading reconciliation.
 */

#include <unity.h>

#include "../../src/ble/scanner/BleWhitelistReconciler.h"

#include <cstring>

using namespace BLE;

namespace {

void setSensor(BLE::BleSensorConfig& sensor, const char* mac, const char* alias) {
    strlcpy(sensor.mac, mac, sizeof(sensor.mac));
    strlcpy(sensor.alias, alias, sizeof(sensor.alias));
}

RTC::BleSensorReading makeReading(uint32_t lastSeenTime,
                                  float temperature,
                                  float humidity,
                                  uint8_t battery,
                                  int8_t rssi) {
    RTC::BleSensorReading reading = detail::makeInvalidReading();
    reading.lastSeenTime = lastSeenTime;
    reading.temperature = temperature;
    reading.humidity = humidity;
    reading.battery = battery;
    reading.rssi = rssi;
    return reading;
}

void setDiscovery(RTC::BleDiscoveryEntry& entry,
                  const char* mac,
                  uint32_t lastSeenTime,
                  float temperature,
                  float humidity,
                  uint8_t battery,
                  int8_t rssi) {
    strlcpy(entry.mac, mac, sizeof(entry.mac));
    entry.lastSeenTime = lastSeenTime;
    entry.temperature = temperature;
    entry.humidity = humidity;
    entry.battery = battery;
    entry.rssi = rssi;
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_reconcile_preserves_existing_reading_by_mac_after_reorder() {
    RTC::BleData state{};
    state.sensorCount = 2;
    setSensor(state.sensors[0], "AA:BB:CC:DD:EE:01", "One");
    setSensor(state.sensors[1], "AA:BB:CC:DD:EE:02", "Two");
    state.readings[0] = makeReading(1000, 21.5f, 45.0f, 80, -61);
    state.readings[1] = makeReading(2000, 26.0f, 51.0f, 70, -58);

    BLE::BleSensorConfig nextSensors[2]{};
    setSensor(nextSensors[0], "AA:BB:CC:DD:EE:02", "Two");
    setSensor(nextSensors[1], "AA:BB:CC:DD:EE:01", "One");

    detail::reconcileWhitelistState(state, nullptr, 0, nextSensors, 2);

    TEST_ASSERT_EQUAL_UINT8(2, state.sensorCount);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:02", state.sensors[0].mac);
    TEST_ASSERT_EQUAL_FLOAT(26.0f, state.readings[0].temperature);
    TEST_ASSERT_EQUAL_UINT32(2000, state.readings[0].lastSeenTime);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:01", state.sensors[1].mac);
    TEST_ASSERT_EQUAL_FLOAT(21.5f, state.readings[1].temperature);
    TEST_ASSERT_EQUAL_UINT32(1000, state.readings[1].lastSeenTime);
}

void test_reconcile_seeds_new_sensor_from_discovery_cache_when_runtime_slot_is_missing() {
    RTC::BleData state{};
    state.sensorCount = 1;
    setSensor(state.sensors[0], "AA:BB:CC:DD:EE:01", "One");
    state.readings[0] = makeReading(1000, 21.0f, 44.0f, 81, -63);

    RTC::BleDiscoveryEntry discovered[RTC::kMaxBleDiscovered]{};
    setDiscovery(discovered[0], "AA:BB:CC:DD:EE:02", 3456, 24.5f, 53.0f, 66, -57);

    BLE::BleSensorConfig nextSensors[2]{};
    setSensor(nextSensors[0], "AA:BB:CC:DD:EE:01", "One");
    setSensor(nextSensors[1], "AA:BB:CC:DD:EE:02", "Two");

    detail::reconcileWhitelistState(state, discovered, RTC::kMaxBleDiscovered, nextSensors, 2);

    TEST_ASSERT_EQUAL_UINT8(2, state.sensorCount);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:02", state.sensors[1].mac);
    TEST_ASSERT_EQUAL_FLOAT(24.5f, state.readings[1].temperature);
    TEST_ASSERT_EQUAL_FLOAT(53.0f, state.readings[1].humidity);
    TEST_ASSERT_EQUAL_UINT8(66, state.readings[1].battery);
    TEST_ASSERT_EQUAL_INT8(-57, state.readings[1].rssi);
    TEST_ASSERT_EQUAL_UINT32(3456, state.readings[1].lastSeenTime);
}

void test_reconcile_clears_slot_when_sensor_has_no_previous_or_discovery_reading() {
    RTC::BleData state{};
    RTC::BleDiscoveryEntry discovered[RTC::kMaxBleDiscovered]{};
    BLE::BleSensorConfig nextSensors[1]{};
    setSensor(nextSensors[0], "AA:BB:CC:DD:EE:03", "Three");

    detail::reconcileWhitelistState(state, discovered, RTC::kMaxBleDiscovered, nextSensors, 1);

    TEST_ASSERT_EQUAL_UINT8(1, state.sensorCount);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:03", state.sensors[0].mac);
    TEST_ASSERT_EQUAL_UINT32(0, state.readings[0].lastSeenTime);
    TEST_ASSERT_EQUAL_FLOAT(-999.0f, state.readings[0].temperature);
    TEST_ASSERT_FALSE(detail::hasValidReading(state.readings[0]));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_reconcile_preserves_existing_reading_by_mac_after_reorder);
    RUN_TEST(test_reconcile_seeds_new_sensor_from_discovery_cache_when_runtime_slot_is_missing);
    RUN_TEST(test_reconcile_clears_slot_when_sensor_has_no_previous_or_discovery_reading);
    return UNITY_END();
}
