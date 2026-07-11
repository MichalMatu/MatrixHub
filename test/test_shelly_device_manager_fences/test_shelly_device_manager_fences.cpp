#include <unity.h>

#include <cstdarg>
#include <functional>
#include <thread>

#define private public
#include "../../src/shelly/device/ShellyDeviceManager.h"
#undef private

// Native tests do not build production sources automatically.
#include "../../src/shelly/device/ShellyDeviceStore.cpp"
#include "../../src/shelly/device/ShellyDeviceValidator.cpp"
#include "../../src/shelly/device/ShellyDeviceManager.cpp"
#include "../../src/shelly/validation/IpValidator.cpp"

namespace {

RTC::ConfigStore gRtcConfig{};
RTC::ShellyData gShellyConfig{};

SHELLY::ShellyDevice makeDevice(const char* ip) {
    SHELLY::ShellyDevice device;
    strlcpy(device.id, "relay-a", sizeof(device.id));
    strlcpy(device.name, "Relay A", sizeof(device.name));
    strlcpy(device.ip, ip, sizeof(device.ip));
    device.enabled = true;
    device.generation = 2;
    return device;
}

}  // namespace

namespace LOG {

void Logging::log(esp_log_level_t, const char*, const char*, ...) {}

}  // namespace LOG

namespace RTC {

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(gRtcConfig);
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    updater(gRtcConfig);
    return true;
}

}  // namespace RTC

namespace SHELLY::CONFIG_STORE {

RTC::ShellyData copy() {
    return gShellyConfig;
}

void withConfig(const std::function<void(const RTC::ShellyData&)>& reader) {
    reader(gShellyConfig);
}

bool update(const std::function<void(RTC::ShellyData&)>& updater) {
    updater(gShellyConfig);
    return true;
}

}  // namespace SHELLY::CONFIG_STORE

namespace CONFIG {

bool save(FS&) {
    return true;
}

}  // namespace CONFIG

namespace SHELLY {

ShellyRepository::ShellyRepository(FS& fs) : _fs(fs) {}

}  // namespace SHELLY

void setUp() {
    gRtcConfig = RTC::ConfigStore{};
    gShellyConfig = RTC::ShellyData{};
    TEST_STUBS::ARDUINO::millisValue = 100;
}

void tearDown() {}

void test_lookup_distinguishes_mutex_busy_from_not_found() {
    FS fs;
    SHELLY::ShellyDeviceManager manager(fs);
    SHELLY::ShellyDevice out;

    TEST_ASSERT_EQUAL(
        SHELLY::ShellyDeviceLookupResult::NotFound,
        manager.lookupDevice("missing", out, 0));

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(manager._mutex, portMAX_DELAY));
    SHELLY::ShellyDeviceLookupResult result =
        SHELLY::ShellyDeviceLookupResult::Found;
    std::thread contender([&]() {
        result = manager.lookupDevice("missing", out, pdMS_TO_TICKS(1));
    });
    contender.join();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(manager._mutex));
    TEST_ASSERT_EQUAL(SHELLY::ShellyDeviceLookupResult::Busy, result);
}

void test_stale_poll_cannot_overwrite_reconfigured_peer() {
    FS fs;
    SHELLY::ShellyDeviceManager manager(fs);
    const SHELLY::ShellyDevice oldPeer = makeDevice("192.168.1.20");
    TEST_ASSERT_TRUE(manager.upsertDevice(oldPeer));

    SHELLY::ShellyDevice newPeer = oldPeer;
    strlcpy(newPeer.ip, "192.168.1.21", sizeof(newPeer.ip));
    TEST_ASSERT_TRUE(manager.upsertDevice(newPeer));

    SHELLY::ShellyStatus staleStatus;
    staleStatus.isOn = true;
    staleStatus.hasPower = true;
    staleStatus.power = 42.0f;
    TEST_ASSERT_FALSE(manager.updatePollState(oldPeer, staleStatus, true));

    SHELLY::ShellyDevice current;
    TEST_ASSERT_TRUE(manager.getDevice("relay-a", current));
    TEST_ASSERT_EQUAL_STRING("192.168.1.21", current.ip);
    TEST_ASSERT_FALSE(current.isOnline);
    TEST_ASSERT_FALSE(current.isOn);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, current.power);

    TEST_ASSERT_TRUE(manager.updatePollState(newPeer, staleStatus, true));
    TEST_ASSERT_TRUE(manager.getDevice("relay-a", current));
    TEST_ASSERT_TRUE(current.isOnline);
    TEST_ASSERT_TRUE(current.isOn);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.0f, current.power);
}

void test_zero_power_debounce_is_committed_inside_same_peer_fence() {
    FS fs;
    SHELLY::ShellyDeviceManager manager(fs);
    const SHELLY::ShellyDevice peer = makeDevice("192.168.1.30");
    TEST_ASSERT_TRUE(manager.upsertDevice(peer));

    SHELLY::ShellyStatus powered;
    powered.isOn = true;
    powered.hasPower = true;
    powered.power = 15.0f;
    TEST_ASSERT_TRUE(manager.updatePollState(peer, powered, true));

    SHELLY::ShellyStatus zero = powered;
    zero.power = 0.0f;
    SHELLY::ShellyDevice current;
    TEST_ASSERT_FALSE(manager.updatePollState(peer, zero, true));
    TEST_ASSERT_TRUE(manager.getDevice("relay-a", current));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, current.power);
    TEST_ASSERT_EQUAL_UINT8(1, current.zeroPowerCount);

    TEST_ASSERT_FALSE(manager.updatePollState(peer, zero, true));
    TEST_ASSERT_TRUE(manager.getDevice("relay-a", current));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 15.0f, current.power);
    TEST_ASSERT_EQUAL_UINT8(2, current.zeroPowerCount);

    TEST_ASSERT_TRUE(manager.updatePollState(peer, zero, true));
    TEST_ASSERT_TRUE(manager.getDevice("relay-a", current));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, current.power);
    TEST_ASSERT_EQUAL_UINT8(0, current.zeroPowerCount);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_lookup_distinguishes_mutex_busy_from_not_found);
    RUN_TEST(test_stale_poll_cannot_overwrite_reconfigured_peer);
    RUN_TEST(test_zero_power_debounce_is_committed_inside_same_peer_fence);
    return UNITY_END();
}
