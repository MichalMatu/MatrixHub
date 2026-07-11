#include <unity.h>

#include <array>
#include <atomic>
#include <cstdarg>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "../../src/core/config/ConfigManager.h"
#include "../../src/gpio/GpioConfigStore.h"
#include "../../src/gpio/GpioSafePins.h"
#include "../../src/gpio/GpioService.h"
#include "../../src/system/logging/Logging.h"

namespace {

GPIO::GpioData gConfig{};
std::array<int, 64> gPinLevels{};

struct ObservedLevel {
    std::string id;
    bool value = false;
};

std::vector<ObservedLevel> gObserved;

void observeOnly(const char* expectedId, const char* id, bool value) {
    if (std::strcmp(expectedId, id) == 0) {
        gObserved.push_back({id, value});
    }
}

}  // namespace

extern "C" void pinMode(uint8_t, uint8_t) {}

extern "C" void digitalWrite(uint8_t pin, uint8_t value) {
    if (pin < gPinLevels.size()) {
        gPinLevels[pin] = value;
    }
}

extern "C" int digitalRead(uint8_t pin) {
    return pin < gPinLevels.size() ? gPinLevels[pin] : LOW;
}

extern "C" void delayMicroseconds(uint32_t) {}

namespace LOG {

void Logging::log(esp_log_level_t, const char*, const char*, ...) {}

}  // namespace LOG

namespace CONFIG {

bool save(FS&) {
    return true;
}

}  // namespace CONFIG

namespace GPIO::CONFIG_STORE {

GpioData copy() {
    return gConfig;
}

void withConfig(const std::function<void(const GpioData&)>& reader) {
    reader(gConfig);
}

bool update(const std::function<void(GpioData&)>& updater) {
    updater(gConfig);
    return true;
}

bool resetToDefaults() {
    applyDefaultConfig(gConfig);
    return true;
}

}  // namespace GPIO::CONFIG_STORE

// The native environment intentionally does not compile production sources.
#include "../../src/gpio/GpioTypes.cpp"
#include "../../src/gpio/GpioSafePins.cpp"
#include "../../src/gpio/GpioService.cpp"

void setUp() {
    GPIO::applyDefaultConfig(gConfig);
    gPinLevels.fill(LOW);
    gObserved.clear();
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
}

void tearDown() {}

void test_debounced_input_publishes_complete_pulse_without_waiting_for_alarm_loop() {
    gConfig.channels[0].mode = GPIO::GpioMode::Input;
    gConfig.channels[0].debounceMs = 0;

    GPIO::GpioService service(nullptr);
    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_TRUE(service.setInputChangeCallback(
        [](const char* id, bool value) { observeOnly("gpio1", id, value); }));
    TEST_ASSERT_EQUAL_UINT32(1, gObserved.size());
    TEST_ASSERT_FALSE(gObserved[0].value);

    gPinLevels[1] = HIGH;
    service.sampleInputsOnceForTest(20);
    service.sampleInputsOnceForTest(20);
    gPinLevels[1] = LOW;
    service.sampleInputsOnceForTest(40);
    service.sampleInputsOnceForTest(40);

    TEST_ASSERT_EQUAL_UINT32(3, gObserved.size());
    TEST_ASSERT_TRUE(gObserved[1].value);
    TEST_ASSERT_FALSE(gObserved[2].value);
}

void test_output_change_and_config_disable_publish_logical_edges() {
    gConfig.channels[1].mode = GPIO::GpioMode::Output;
    gConfig.channels[1].initialOutput = false;

    GPIO::GpioService service(nullptr);
    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_TRUE(service.setInputChangeCallback(
        [](const char* id, bool value) { observeOnly("gpio2", id, value); }));
    TEST_ASSERT_EQUAL_UINT32(1, gObserved.size());
    TEST_ASSERT_FALSE(gObserved[0].value);

    TEST_ASSERT_TRUE(service.setOutput("gpio2", true, false));
    TEST_ASSERT_TRUE(service.setOutput("gpio2", true, false));
    TEST_ASSERT_EQUAL_UINT32(2, gObserved.size());
    TEST_ASSERT_TRUE(gObserved[1].value);

    GPIO::GpioData disabled = gConfig;
    disabled.channels[1].mode = GPIO::GpioMode::Disabled;
    TEST_ASSERT_TRUE(service.updateConfig(disabled, false));
    TEST_ASSERT_EQUAL_UINT32(3, gObserved.size());
    TEST_ASSERT_FALSE(gObserved[2].value);
}

void test_removed_high_selector_publishes_terminal_clear() {
    gConfig.channels[0].mode = GPIO::GpioMode::Input;
    gConfig.channels[0].debounceMs = 0;
    gPinLevels[1] = HIGH;

    GPIO::GpioService service(nullptr);
    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_TRUE(service.setInputChangeCallback(
        [](const char* id, bool value) { observeOnly("gpio1", id, value); }));
    TEST_ASSERT_EQUAL_UINT32(1, gObserved.size());
    TEST_ASSERT_TRUE(gObserved[0].value);

    GPIO::GpioData emptyRuntime{};
    TEST_ASSERT_TRUE(service.applyRuntimeConfigForTest(emptyRuntime, 20));
    TEST_ASSERT_EQUAL_UINT32(2, gObserved.size());
    TEST_ASSERT_FALSE(gObserved[1].value);
}

void test_detach_waits_for_in_flight_callback() {
    gConfig.channels[0].mode = GPIO::GpioMode::Input;
    gConfig.channels[0].debounceMs = 0;

    GPIO::GpioService service(nullptr);
    TEST_ASSERT_TRUE(service.begin());

    std::atomic<bool> callbackEntered{false};
    std::atomic<bool> releaseCallback{false};
    TEST_ASSERT_TRUE(service.setInputChangeCallback(
        [&](const char* id, bool value) {
            if (std::strcmp(id, "gpio1") == 0 && value) {
                callbackEntered.store(true, std::memory_order_release);
                while (!releaseCallback.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            }
        }));

    gPinLevels[1] = HIGH;
    service.sampleInputsOnceForTest(20);
    std::thread publisher([&]() { service.sampleInputsOnceForTest(20); });
    while (!callbackEntered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    std::atomic<bool> detachStarted{false};
    std::atomic<bool> detachReturned{false};
    std::thread detacher([&]() {
        detachStarted.store(true, std::memory_order_release);
        (void)service.setInputChangeCallback(nullptr);
        detachReturned.store(true, std::memory_order_release);
    });
    while (!detachStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    for (uint16_t i = 0; i < 1000; ++i) {
        std::this_thread::yield();
    }
    TEST_ASSERT_FALSE(detachReturned.load(std::memory_order_acquire));

    releaseCallback.store(true, std::memory_order_release);
    publisher.join();
    detacher.join();
    TEST_ASSERT_TRUE(detachReturned.load(std::memory_order_acquire));
}

void test_detached_callback_receives_no_later_input_edge() {
    gConfig.channels[0].mode = GPIO::GpioMode::Input;
    gConfig.channels[0].debounceMs = 0;

    GPIO::GpioService service(nullptr);
    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_TRUE(service.setInputChangeCallback(
        [](const char* id, bool value) { observeOnly("gpio1", id, value); }));
    service.stop();

    gPinLevels[1] = HIGH;
    service.sampleInputsOnceForTest(20);
    service.sampleInputsOnceForTest(20);
    TEST_ASSERT_EQUAL_UINT32(1, gObserved.size());
}

void test_begin_rejects_invalid_normalized_config_before_starting_task() {
    gConfig.channels[0].pin = 63;

    GPIO::GpioService service(nullptr);

    TEST_ASSERT_FALSE(service.begin());
    TEST_ASSERT_FALSE(service.isRunning());
    TEST_ASSERT_NULL(TEST_STUBS::FREERTOS::lastTaskFunction);
}

void test_begin_propagates_task_creation_failure() {
    TEST_STUBS::FREERTOS::taskCreateResult = pdFAIL;

    GPIO::GpioService service(nullptr);

    TEST_ASSERT_FALSE(service.begin());
    TEST_ASSERT_FALSE(service.isRunning());
    TEST_ASSERT_NULL(TEST_STUBS::FREERTOS::lastDeletedTask);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_debounced_input_publishes_complete_pulse_without_waiting_for_alarm_loop);
    RUN_TEST(test_output_change_and_config_disable_publish_logical_edges);
    RUN_TEST(test_removed_high_selector_publishes_terminal_clear);
    RUN_TEST(test_detach_waits_for_in_flight_callback);
    RUN_TEST(test_detached_callback_receives_no_later_input_edge);
    RUN_TEST(test_begin_rejects_invalid_normalized_config_before_starting_task);
    RUN_TEST(test_begin_propagates_task_creation_failure);
    return UNITY_END();
}
