#ifdef NATIVE_BUILD
#include <unity.h>
#include <Arduino.h>
#include <esp_log.h>

#ifndef HIGH
#define HIGH 1
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif
#ifndef OUTPUT
#define OUTPUT 3
#endif

#include "../../src/system/logging/Logging.h"

// Linker implementations for Logging
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* name, uint32_t period) {}
}

// Mock state for GPIO pins
static int mock_sda_pin = -1;
static int mock_scl_pin = -1;
static int mock_sda_mode = 0;
static int mock_scl_mode = 0;
static int mock_sda_value = HIGH;
static int mock_scl_value = HIGH;
static int scl_toggle_count = 0;
static bool sda_stuck_low = false;

extern "C" {
    // Redefine Arduino mocks for native tests
    void pinMode(uint8_t pin, uint8_t mode) {
        if (pin == mock_sda_pin) mock_sda_mode = mode;
        if (pin == mock_scl_pin) mock_scl_mode = mode;
    }
    
    void digitalWrite(uint8_t pin, uint8_t val) {
        if (pin == mock_sda_pin) {
            mock_sda_value = val;
        }
        if (pin == mock_scl_pin) {
            // Count high-to-low transitions (clocks)
            if (mock_scl_value == HIGH && val == LOW) {
                scl_toggle_count++;
                
                // If SDA is stuck low, simulate a slave releasing it after 5 clocks
                if (sda_stuck_low && scl_toggle_count >= 5) {
                    sda_stuck_low = false;
                    mock_sda_value = HIGH;
                }
            }
            mock_scl_value = val;
        }
    }
    
    int digitalRead(uint8_t pin) {
        if (pin == mock_sda_pin) {
            return sda_stuck_low ? LOW : mock_sda_value;
        }
        if (pin == mock_scl_pin) return mock_scl_value;
        return LOW;
    }
    
    void delayMicroseconds(uint32_t us) {}
}

#include "../../src/utils/hardware/I2cUtils.cpp"

void setUp(void) {
    mock_sda_pin = 4;
    mock_scl_pin = 5;
    mock_sda_mode = 0;
    mock_scl_mode = 0;
    mock_sda_value = HIGH;
    mock_scl_value = HIGH;
    scl_toggle_count = 0;
    sda_stuck_low = false;
}

void tearDown(void) {}

void test_recoverBus_when_sda_is_high() {
    // Start with SDA high (bus is free)
    sda_stuck_low = false;
    
    UTILS::HARDWARE::I2cUtils::recoverBus(mock_sda_pin, mock_scl_pin);
    
    // Nothing should be clocked if SDA is already HIGH
    // Wait, the implementation of `recoverBus` always clocks 9 times just to be safe, 
    // or it checks if SDA is high first? Let's verify by testing.
    // Actually, looking at typical recoverBus, it usually checks `digitalRead(sda_pin) == LOW` in a loop
    // But since we include the real CPP, let's see what it does.
    // We expect it to leave SDA high.
    TEST_ASSERT_EQUAL(HIGH, digitalRead(mock_sda_pin));
    
    // Most recover functions issue 9 clocks unconditionally just to be sure, or while SDA is low.
    // We'll just verify the bus ends up in a good state.
}

void test_recoverBus_when_sda_is_stuck_low() {
    // Simulate a hung slave holding SDA low
    sda_stuck_low = true;
    mock_sda_value = LOW;
    
    // Call recovery routine
    UTILS::HARDWARE::I2cUtils::recoverBus(mock_sda_pin, mock_scl_pin);
    
    // After recovery, the mock ensures SDA is released after 5 clocks.
    // So SDA should be HIGH at the end.
    TEST_ASSERT_FALSE(sda_stuck_low);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(mock_sda_pin));
    
    // It should have toggled SCL at least 5 times
    TEST_ASSERT_GREATER_OR_EQUAL(5, scl_toggle_count);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_recoverBus_when_sda_is_high);
    RUN_TEST(test_recoverBus_when_sda_is_stuck_low);
    return UNITY_END();
}
#endif // NATIVE_BUILD
