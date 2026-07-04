#define NOMINMAX
#include <unity.h>
#include <ArduinoJson.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include "../../src/config/json/ShellyConfigJson.cpp"
#include "../../src/system/rtc/types/RtcShellyTypes.h"

namespace RTC {
    static ConfigStore mockStore;

    const ConfigStore& getConfig() { return mockStore; }
    ConfigStore& getMutableConfig() { return mockStore; }
    void withConfig(const std::function<void(const ConfigStore&)>& reader) { reader(mockStore); }
    bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
        updater(mockStore);
        return true;
    }
}

namespace SHELLY {
namespace CONFIG_STORE {
    static RTC::ShellyData mockData;

    RTC::ShellyData copy() { return mockData; }
    void withConfig(const std::function<void(const RTC::ShellyData&)>& reader) { reader(mockData); }
    bool update(const std::function<void(RTC::ShellyData&)>& updater) {
        updater(mockData);
        return true;
    }
}
}

void setUp(void) {
    RTC::mockStore.shelly = RTC::ShellySummaryData();
    SHELLY::CONFIG_STORE::mockData = RTC::ShellyData();
}

void tearDown(void) {}

void test_load_shelly_reads_generation() {
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    JsonObject dev = devices.add<JsonObject>();
    dev["id"] = "gen1";
    dev["ip"] = "192.168.1.10";
    dev["name"] = "Shelly 1";
    dev["relay_index"] = 2;
    dev["generation"] = 1;
    dev["enabled"] = true;

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadShelly(obj);

    const auto shelly = SHELLY::CONFIG_STORE::copy();
    TEST_ASSERT_EQUAL_UINT8(1, RTC::mockStore.shelly.deviceCount);
    TEST_ASSERT_EQUAL_UINT8(1, RTC::mockStore.shelly.enabledCount);
    TEST_ASSERT_EQUAL_UINT8(1, shelly.deviceCount);
    TEST_ASSERT_EQUAL_UINT8(2, shelly.devices[0].relayIndex);
    TEST_ASSERT_EQUAL_UINT8(1, shelly.devices[0].generation);
    TEST_ASSERT_TRUE(shelly.devices[0].enabled);
}

void test_load_shelly_invalid_generation_defaults_to_gen2() {
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    JsonObject dev = devices.add<JsonObject>();
    dev["id"] = "bad-gen";
    dev["ip"] = "192.168.1.11";
    dev["generation"] = 9;

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadShelly(obj);

    const auto shelly = SHELLY::CONFIG_STORE::copy();
    TEST_ASSERT_EQUAL_UINT8(1, RTC::mockStore.shelly.deviceCount);
    TEST_ASSERT_EQUAL_UINT8(1, RTC::mockStore.shelly.enabledCount);
    TEST_ASSERT_EQUAL_UINT8(2, shelly.devices[0].generation);
}

void test_load_shelly_legacy_missing_generation_defaults_to_gen1() {
    JsonDocument doc;
    JsonArray devices = doc["devices"].to<JsonArray>();
    JsonObject dev = devices.add<JsonObject>();
    dev["id"] = "legacy";
    dev["ip"] = "192.168.1.15";
    dev["relay_index"] = 1;
    dev["enabled"] = true;

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadShelly(obj);

    const auto shelly = SHELLY::CONFIG_STORE::copy();
    TEST_ASSERT_EQUAL_UINT8(1, RTC::mockStore.shelly.deviceCount);
    TEST_ASSERT_EQUAL_UINT8(1, RTC::mockStore.shelly.enabledCount);
    TEST_ASSERT_EQUAL_UINT8(1, shelly.devices[0].generation);
}

void test_save_shelly_persists_generation_and_omits_poll_interval() {
    auto& shelly = SHELLY::CONFIG_STORE::mockData;
    shelly.deviceCount = 1;
    auto& dev = shelly.devices[0];
    strlcpy(dev.id, "saved", sizeof(dev.id));
    strlcpy(dev.ip, "192.168.1.12", sizeof(dev.ip));
    strlcpy(dev.name, "Saved Shelly", sizeof(dev.name));
    dev.relayIndex = 3;
    dev.generation = 1;
    dev.enabled = true;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    CONFIG::JSON::saveShelly(obj);

    TEST_ASSERT_TRUE(obj["poll_interval_ms"].isNull());
    TEST_ASSERT_TRUE(obj["devices"].is<JsonArray>());
    TEST_ASSERT_EQUAL_UINT8(1, obj["devices"][0]["generation"].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(3, obj["devices"][0]["relay_index"].as<uint8_t>());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_load_shelly_reads_generation);
    RUN_TEST(test_load_shelly_invalid_generation_defaults_to_gen2);
    RUN_TEST(test_load_shelly_legacy_missing_generation_defaults_to_gen1);
    RUN_TEST(test_save_shelly_persists_generation_and_omits_poll_interval);
    return UNITY_END();
}
