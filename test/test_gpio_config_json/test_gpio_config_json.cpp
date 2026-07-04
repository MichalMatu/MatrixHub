#include <unity.h>
#include <ArduinoJson.h>
#include <functional>

#include "../../src/gpio/GpioTypes.cpp"
#include "../../src/gpio/GpioSafePins.cpp"

namespace GPIO {
namespace CONFIG_STORE {
GpioData copy() {
    GpioData data;
    applyDefaultConfig(data);
    return data;
}

bool update(const std::function<void(GpioData&)>& updater) {
    GpioData data;
    applyDefaultConfig(data);
    updater(data);
    return true;
}
}  // namespace CONFIG_STORE
}  // namespace GPIO

#include "../../src/config/json/GpioConfigJson.cpp"

void setUp(void) {}
void tearDown(void) {}

void test_default_config_uses_conservative_allow_list() {
    GPIO::GpioData data;
    GPIO::applyDefaultConfig(data);

    TEST_ASSERT_EQUAL_UINT8(7, data.channelCount);
    TEST_ASSERT_EQUAL_UINT8(1, data.channels[0].pin);
    TEST_ASSERT_EQUAL_UINT8(2, data.channels[1].pin);
    TEST_ASSERT_EQUAL_UINT8(4, data.channels[2].pin);
    TEST_ASSERT_EQUAL_UINT8(5, data.channels[3].pin);
    TEST_ASSERT_EQUAL_UINT8(38, data.channels[4].pin);
    TEST_ASSERT_EQUAL_UINT8(39, data.channels[5].pin);
    TEST_ASSERT_EQUAL_UINT8(40, data.channels[6].pin);
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(0));
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(3));
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(6));
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(7));
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(33));
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(14));
    TEST_ASSERT_FALSE(GPIO::isPinAllowed(43));
}

void test_deserialize_gpio_accepts_safe_input_channel() {
    JsonDocument doc;
    JsonArray channels = doc["channels"].to<JsonArray>();
    JsonObject ch = channels.add<JsonObject>();
    ch["id"] = "gpio38";
    ch["pin"] = 38;
    ch["name"] = "Door";
    ch["mode"] = "input";
    ch["pull"] = "up";
    ch["inverted"] = true;
    ch["debounce_ms"] = 75;

    GPIO::GpioData data;
    JsonObject root = doc.as<JsonObject>();
    TEST_ASSERT_TRUE(CONFIG::JSON::deserializeGpio(root, data));
    TEST_ASSERT_EQUAL_STRING("Door", data.channels[4].name);
    TEST_ASSERT_EQUAL(GPIO::GpioMode::Input, data.channels[4].mode);
    TEST_ASSERT_EQUAL(GPIO::GpioPull::Up, data.channels[4].pull);
    TEST_ASSERT_TRUE(data.channels[4].inverted);
    TEST_ASSERT_EQUAL_UINT16(75, data.channels[4].debounceMs);
}

void test_deserialize_gpio_rejects_pin_number_change() {
    JsonDocument doc;
    JsonArray channels = doc["channels"].to<JsonArray>();
    JsonObject ch = channels.add<JsonObject>();
    ch["id"] = "gpio38";
    ch["pin"] = 14;
    ch["mode"] = "input";

    GPIO::GpioData data;
    JsonObject root = doc.as<JsonObject>();
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeGpio(root, data));
}

void test_deserialize_gpio_rejects_unknown_pin_id() {
    JsonDocument doc;
    JsonArray channels = doc["channels"].to<JsonArray>();
    JsonObject ch = channels.add<JsonObject>();
    ch["id"] = "gpio6";
    ch["pin"] = 6;
    ch["mode"] = "input";

    GPIO::GpioData data;
    JsonObject root = doc.as<JsonObject>();
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeGpio(root, data));
}

void test_deserialize_gpio_rejects_invalid_mode() {
    JsonDocument doc;
    JsonArray channels = doc["channels"].to<JsonArray>();
    JsonObject ch = channels.add<JsonObject>();
    ch["id"] = "gpio38";
    ch["pin"] = 38;
    ch["mode"] = "pwm";

    GPIO::GpioData data;
    JsonObject root = doc.as<JsonObject>();
    TEST_ASSERT_FALSE(CONFIG::JSON::deserializeGpio(root, data));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_default_config_uses_conservative_allow_list);
    RUN_TEST(test_deserialize_gpio_accepts_safe_input_channel);
    RUN_TEST(test_deserialize_gpio_rejects_pin_number_change);
    RUN_TEST(test_deserialize_gpio_rejects_unknown_pin_id);
    RUN_TEST(test_deserialize_gpio_rejects_invalid_mode);
    return UNITY_END();
}
