#include <unity.h>

#include "../../src/system/rtc/RtcDefaultValues.h"
#include "../../src/system/rtc/types/RtcSystemTypes.h"
#include "../../src/system/rtc/types/RtcBleTypes.h"
#include "../../src/system/rtc/types/RtcAirMouseTypes.h"
#include "../../src/system/rtc/types/RtcMatrixTypes.h"
#include "../../src/system/rtc/types/RtcMacroTypes.h"
#include "../../src/system/rtc/types/RtcKeyboardTypes.h"
#include "../../src/system/rtc/types/RtcCompensationTypes.h"
#include "../../src/system/rtc/types/RtcWifiSensingTypes.h"
#include "../../src/system/rtc/types/RtcUsbTerminalTypes.h"

void setUp(void) {}
void tearDown(void) {}

void test_ble_data_defaults_match_rtc_defaults() {
    RTC::BleData data{};

    TEST_ASSERT_EQUAL(RTC::Defaults::Ble::Enabled, data.enabled);
    TEST_ASSERT_EQUAL_UINT8(0, data.sensorCount);
    TEST_ASSERT_EQUAL('\0', data.sensors[0].mac[0]);
    TEST_ASSERT_EQUAL('\0', data.sensors[0].alias[0]);
}

void test_logging_and_power_defaults_match_rtc_defaults() {
    RTC::LoggingData logging{};
    RTC::PowerData power{};

    TEST_ASSERT_EQUAL(RTC::Defaults::Logging::Level, logging.level);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Power::InactivityTimeoutMs, power.inactivityTimeoutMs);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Power::GraceAfterBootMs, power.graceAfterBootMs);
    TEST_ASSERT_EQUAL(RTC::Defaults::Power::SleepEnabled, power.sleepEnabled);
    TEST_ASSERT_EQUAL(RTC::Defaults::Power::WakeTimerEnabled, power.wakeTimerEnabled);
    TEST_ASSERT_EQUAL(RTC::Defaults::Power::WakeButtonEnabled, power.wakeButtonEnabled);
    TEST_ASSERT_EQUAL(RTC::Defaults::Power::WakeTouchEnabled, power.wakeTouchEnabled);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Power::WakeIntervalMs, power.wakeIntervalMs);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Power::TimerWakeAwakeMs, power.timerWakeAwakeMs);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Power::ButtonWakeAwakeMs, power.buttonWakeAwakeMs);
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::Power::TouchWakeGpio, power.wakeTouchGpio);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Power::TouchWakeThreshold, power.wakeTouchThreshold);
}

void test_heartbeat_and_udp_defaults_match_rtc_defaults() {
    RTC::HeartbeatData heartbeat{};
    RTC::UdpPusherData udp{};

    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Heartbeat::IntervalMs, heartbeat.intervalMs);
    TEST_ASSERT_FALSE(heartbeat.slots[0].enabled);
    TEST_ASSERT_EQUAL(RTC::Defaults::UdpPusher::Enabled, udp.enabled);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::UdpPusher::Port, udp.port);
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::UdpPusher::Format, static_cast<uint8_t>(udp.format));
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::UdpPusher::IntervalMs, udp.intervalMs);
}

void test_air_mouse_defaults_match_rtc_defaults() {
    RTC::AirMouseData data{};

    TEST_ASSERT_EQUAL(RTC::Defaults::AirMouse::MovementEnabled, data.movementEnabled);
    TEST_ASSERT_EQUAL(RTC::Defaults::AirMouse::ClickEnabled, data.clickEnabled);
    // Transport selection was removed from the live AirMouse contract. Keep the
    // retained byte pinned to its default value until RTC schema cleanup drops it.
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::AirMouse::ReservedTransport, data.reservedTransport);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::SensitivityX, data.sensitivityX);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::SensitivityY, data.sensitivityY);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::Deadzone, data.deadzone);
    TEST_ASSERT_EQUAL(RTC::Defaults::AirMouse::AccelerationEnabled, data.accelerationEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::AccelerationFactor, data.accelerationFactor);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::TapThresholdG, data.tapThresholdG);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::AirMouse::ClickDebounceMs, data.clickDebounceMs);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::AirMouse::DoubleClickWindowMs, data.doubleClickWindowMs);
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::AirMouse::ClickSource, static_cast<uint8_t>(data.clickSource));
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::AirMouse::SingleClickAction, static_cast<uint8_t>(data.singleClickAction));
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::AirMouse::DoubleClickAction, static_cast<uint8_t>(data.doubleClickAction));
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::AirMouse::TripleClickAction, static_cast<uint8_t>(data.tripleClickAction));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::EuroMinCutoff, data.euroMinCutoff);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::EuroBeta, data.euroBeta);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::AirMouse::EuroDCutoff, data.euroDCutoff);
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::AirMouse::Jiggler::Mode, static_cast<uint8_t>(data.jiggler.mode));
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::AirMouse::Jiggler::IntervalS, data.jiggler.interval);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::AirMouse::Jiggler::MoveDistance, data.jiggler.moveDistance);
    TEST_ASSERT_EQUAL(RTC::Defaults::AirMouse::Jiggler::RandomInterval, data.jiggler.randomInterval);
}

void test_matrix_data_defaults_match_rtc_defaults() {
    RTC::MatrixData data{};

    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::Matrix::Brightness, data.brightness);
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::Matrix::AlarmMode, static_cast<uint8_t>(data.alarmMode));
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::Matrix::Rotation, data.rotation);
    TEST_ASSERT_EQUAL(RTC::Defaults::Matrix::AutoRotate, data.autoRotate);
    TEST_ASSERT_EQUAL(RTC::Defaults::Matrix::EffectEnabled, data.effectEnabled);
    TEST_ASSERT_EQUAL_UINT8(RTC::Defaults::Matrix::EffectMode, data.effectMode);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::Matrix::EffectSpeed, data.effectSpeed);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Matrix::EffectColor, data.effectColor);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Matrix::EffectColor2, data.effectColor2);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Matrix::EffectColor3, data.effectColor3);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Matrix::Menu::TextColor, data.menu.textColor);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::Matrix::Menu::ScrollSpeed, data.menu.scrollSpeed);
    TEST_ASSERT_EQUAL(RTC::Defaults::Matrix::Menu::Enabled, data.menu.enabled);
}

void test_wifi_sensing_defaults_match_rtc_defaults() {
    RTC::WifiSensingData data{};

    TEST_ASSERT_EQUAL(RTC::Defaults::WifiSensing::Enabled, data.enabled);
    TEST_ASSERT_EQUAL_UINT16(RTC::Defaults::WifiSensing::SampleIntervalMs, data.sampleIntervalMs);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::WifiSensing::VarianceThreshold, data.varianceThreshold);
}

void test_macro_compensation_and_usb_defaults_match_rtc_defaults() {
    RTC::MacroData macro{};
    RTC::KeyboardData keyboard{};
    RTC::CompensationData compensation{};
    RTC::UsbTerminalData usbTerminal{};

    TEST_ASSERT_EQUAL(RTC::Defaults::Macro::Enabled, macro.enabled);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::Macro::BootDelayMs, macro.bootDelay);
    TEST_ASSERT_EQUAL_STRING("", macro.bootScript);

    TEST_ASSERT_EQUAL(RTC::Defaults::Keyboard::Enabled, keyboard.enabled);

    TEST_ASSERT_EQUAL(RTC::Defaults::Compensation::Enabled, compensation.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::Compensation::BaseTempOffset, compensation.baseTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::Compensation::ReferenceCpuTemp, compensation.referenceCpuTemp);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::Compensation::TempOffsetPerCpuDegree, compensation.tempOffsetPerCpuDegree);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::Compensation::MinTempOffset, compensation.minTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, RTC::Defaults::Compensation::MaxTempOffset, compensation.maxTempOffset);

    TEST_ASSERT_EQUAL(RTC::Defaults::UsbTerminal::Enabled, usbTerminal.enabled);
    TEST_ASSERT_EQUAL_UINT32(RTC::Defaults::UsbTerminal::IdleTimeoutMs, usbTerminal.idleTimeoutMs);
    TEST_ASSERT_EQUAL_STRING(RTC::Defaults::UsbTerminal::TargetPort, usbTerminal.targetPort);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_logging_and_power_defaults_match_rtc_defaults);
    RUN_TEST(test_ble_data_defaults_match_rtc_defaults);
    RUN_TEST(test_heartbeat_and_udp_defaults_match_rtc_defaults);
    RUN_TEST(test_air_mouse_defaults_match_rtc_defaults);
    RUN_TEST(test_matrix_data_defaults_match_rtc_defaults);
    RUN_TEST(test_wifi_sensing_defaults_match_rtc_defaults);
    RUN_TEST(test_macro_compensation_and_usb_defaults_match_rtc_defaults);
    return UNITY_END();
}
