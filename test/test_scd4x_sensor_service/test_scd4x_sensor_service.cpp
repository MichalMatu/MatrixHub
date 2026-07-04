#include <unity.h>

#include "../../test/stubs/SensirionI2cScd4x.h"
#include "../../src/utils/hardware/I2cUtils.cpp"
#include "../../src/sensors/scd4x/Scd4xSensorService.cpp"

extern "C" {
void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin;
    (void)mode;
}

void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin;
    (void)val;
}

int digitalRead(uint8_t pin) {
    (void)pin;
    return HIGH;
}

void delayMicroseconds(uint32_t us) {
    (void)us;
}
}

namespace LOG {

Settings Logging::_settings{ESP_LOG_INFO};

void Logging::begin(const Settings& settings) { _settings = settings; }
void Logging::setLevel(esp_log_level_t level) { _settings.level = level; }
Settings Logging::settings() { return _settings; }
bool Logging::isEnabled(esp_log_level_t level) { return level <= _settings.level; }
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::clearBuffer() {}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
void Logging::logSection(const char* title) { (void)title; }
void Logging::suppressNoisyModules() {}

}  // namespace LOG

namespace COMPENSATION {

CompensatedReading CompensationService::compensate(float rawTemp, float rawHumid) {
    return {
        rawTemp,
        rawHumid,
        0.0f,
        0.0f,
        0.0f,
    };
}

}  // namespace COMPENSATION

void setUp() {
    Wire1.reset();
    TEST_STUBS::resetSensirionI2cScd4xState();
}

void tearDown() {}

void test_begin_recovers_from_transient_stop_periodic_i2c_error() {
    TEST_STUBS::queueStopPeriodicMeasurementError(0x010E);
    TEST_STUBS::queueStopPeriodicMeasurementError(0);

    SENSORS::Scd4xSensorService service;

    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_EQUAL(2, TEST_STUBS::sensirionI2cScd4xState.stopPeriodicCalls);
    TEST_ASSERT_EQUAL(1, TEST_STUBS::sensirionI2cScd4xState.reinitCalls);
    TEST_ASSERT_EQUAL(1, TEST_STUBS::sensirionI2cScd4xState.startPeriodicCalls);

    SensorSnapshot snapshot{};
    PhaseStatus status{};
    service.readAll(snapshot, status);

    TEST_ASSERT_TRUE(status.ok);
    TEST_ASSERT_EQUAL_UINT16(650, snapshot.co2);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 23.5f, snapshot.temp);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 48.0f, snapshot.humid);
}

void test_begin_marks_sensor_missing_when_i2c_address_probe_fails() {
    Wire1.queueEndTransmissionResult(2);
    Wire1.queueEndTransmissionResult(2);

    SENSORS::Scd4xSensorService service;

    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_EQUAL(0, TEST_STUBS::sensirionI2cScd4xState.stopPeriodicCalls);

    SensorSnapshot snapshot{};
    PhaseStatus status{};
    service.readAll(snapshot, status);

    TEST_ASSERT_FALSE(status.ok);
    TEST_ASSERT_EQUAL_STRING("SENSOR_MISSING", status.error_code);
}

void test_begin_marks_sensor_missing_after_stop_periodic_retries_fail() {
    TEST_STUBS::queueStopPeriodicMeasurementError(0x010E);
    TEST_STUBS::queueStopPeriodicMeasurementError(0x010E);
    TEST_STUBS::queueStopPeriodicMeasurementError(0x010E);

    SENSORS::Scd4xSensorService service;

    TEST_ASSERT_TRUE(service.begin());
    TEST_ASSERT_EQUAL(3, TEST_STUBS::sensirionI2cScd4xState.stopPeriodicCalls);

    SensorSnapshot snapshot{};
    PhaseStatus status{};
    service.readAll(snapshot, status);

    TEST_ASSERT_FALSE(status.ok);
    TEST_ASSERT_EQUAL_STRING("SENSOR_MISSING", status.error_code);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_begin_recovers_from_transient_stop_periodic_i2c_error);
    RUN_TEST(test_begin_marks_sensor_missing_when_i2c_address_probe_fails);
    RUN_TEST(test_begin_marks_sensor_missing_after_stop_periodic_retries_fail);
    return UNITY_END();
}
