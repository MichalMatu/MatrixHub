#pragma once

#include "../../config/App.h"
#include "../../config/ActionConfig.h"
#include "../../config/UiConfig.h"

#include <cstdint>

namespace RTC::Defaults {

namespace Notification {
constexpr int Mode = FACTORY_NOTIFICATION_MODE;
constexpr bool TelegramEnabled = (Mode == 1 || Mode == 3);
constexpr bool WebhookEnabled = (Mode == 2 || Mode == 3);
constexpr bool PushoverEnabled = false;
constexpr bool CommandsEnabled = FACTORY_TELEGRAM_COMMANDS_ENABLED;
constexpr const char* BotToken = FACTORY_TELEGRAM_BOT_TOKEN;
constexpr const char* ChatId = FACTORY_TELEGRAM_CHAT_ID;
constexpr const char* WebhookUrl = FACTORY_WEBHOOK_URL;
constexpr bool HasBotToken = BotToken[0] != '\0';
constexpr bool HasChatId = ChatId[0] != '\0';
constexpr bool HasWebhookUrl = WebhookUrl[0] != '\0';
constexpr bool HasPushoverCreds = false;
}  // namespace Notification

namespace Logging {
constexpr esp_log_level_t Level = static_cast<esp_log_level_t>(FACTORY_LOG_LEVEL);
}  // namespace Logging

namespace Power {
constexpr uint32_t InactivityTimeoutMs = FACTORY_POWER_INACTIVITY_TIMEOUT_MS;
constexpr uint32_t GraceAfterBootMs = FACTORY_POWER_GRACE_AFTER_BOOT_MS;
constexpr bool SleepEnabled = FACTORY_POWER_SLEEP_ENABLED;
constexpr bool WakeTimerEnabled = FACTORY_POWER_WAKE_TIMER_ENABLED;
constexpr bool WakeButtonEnabled = FACTORY_POWER_WAKE_BUTTON_ENABLED;
constexpr bool WakeTouchEnabled = FACTORY_POWER_WAKE_TOUCH_ENABLED;
constexpr uint32_t WakeIntervalMs = FACTORY_POWER_WAKE_INTERVAL_MS;
constexpr uint32_t TimerWakeAwakeMs = FACTORY_POWER_TIMER_WAKE_AWAKE_MS;
constexpr uint32_t ButtonWakeAwakeMs = FACTORY_POWER_BUTTON_WAKE_AWAKE_MS;
constexpr uint8_t TouchWakeGpio = FACTORY_POWER_TOUCH_WAKE_GPIO;
constexpr uint32_t TouchWakeThreshold = FACTORY_POWER_TOUCH_WAKE_THRESHOLD;
}  // namespace Power

namespace WifiSensing {
constexpr bool Enabled = FACTORY_WIFI_SENSING_ENABLED;
constexpr uint16_t SampleIntervalMs = FACTORY_WIFI_SENSING_SAMPLE_INTERVAL_MS;
constexpr float VarianceThreshold = FACTORY_WIFI_SENSING_VARIANCE_THRESHOLD;
constexpr bool CsiAlarmEnabled = false;
constexpr uint16_t CsiBaselineFrames = 150;
constexpr uint8_t CsiTopK = 8;
constexpr float CsiEnterThreshold = 6.0f;
constexpr float CsiClearThreshold = 3.0f;
constexpr uint16_t CsiHoldMs = 1200;
constexpr uint16_t CsiClearHoldMs = 2500;
constexpr float CsiMinNoise = 4.0f;
constexpr float CsiMinEnergy = 4.0f;
constexpr float CsiNoisyThreshold = 80.0f;
constexpr bool CsiAutoRecalibration = true;
constexpr uint8_t CsiSensitivity = 1;
}  // namespace WifiSensing

namespace Ble {
constexpr bool Enabled = FACTORY_BLE_ENABLED;
}  // namespace Ble

namespace Heartbeat {
constexpr uint32_t IntervalMs = HEARTBEAT::DEFAULT_INTERVAL_MS;
}  // namespace Heartbeat

namespace UdpPusher {
constexpr bool Enabled = false;
constexpr uint16_t Port = 8094;
constexpr uint8_t Format = 0;  // UdpFormat::LineProtocol
constexpr uint32_t IntervalMs = INTEGRATION::UDP::DEFAULT_INTERVAL_MS;
}  // namespace UdpPusher

namespace AirMouse {
constexpr bool MovementEnabled = CONFIG::AIR_MOUSE::DEFAULT_MOVEMENT_ENABLED;
constexpr bool ClickEnabled = CONFIG::AIR_MOUSE::DEFAULT_CLICK_ENABLED;
// Keep one byte reserved so old retained layouts stay binary-compatible even
// though the selectable BLE HID transport was removed from the live contract.
constexpr uint8_t ReservedTransport = 0;
constexpr float SensitivityX = CONFIG::AIR_MOUSE::DEFAULT_SENSITIVITY_X;
constexpr float SensitivityY = CONFIG::AIR_MOUSE::DEFAULT_SENSITIVITY_Y;
constexpr float Deadzone = CONFIG::AIR_MOUSE::DEFAULT_DEADZONE;
constexpr bool AccelerationEnabled = CONFIG::AIR_MOUSE::DEFAULT_ACCELERATION_ENABLED;
constexpr float AccelerationFactor = CONFIG::AIR_MOUSE::DEFAULT_ACCELERATION_FACTOR;
constexpr float TapThresholdG = CONFIG::AIR_MOUSE::DEFAULT_TAP_THRESHOLD_G;
constexpr uint16_t ClickDebounceMs = CONFIG::AIR_MOUSE::DEFAULT_CLICK_DEBOUNCE_MS;
constexpr uint16_t DoubleClickWindowMs = CONFIG::AIR_MOUSE::DEFAULT_DOUBLE_CLICK_WINDOW_MS;
constexpr uint8_t ClickSource = 0;  // ClickSource::SENSOR
constexpr uint8_t SingleClickAction = 1;  // ClickAction::LEFT_CLICK
constexpr uint8_t DoubleClickAction = 2;  // ClickAction::RIGHT_CLICK
constexpr uint8_t TripleClickAction = 0;  // ClickAction::NONE
constexpr float EuroMinCutoff = CONFIG::AIR_MOUSE::FILTER::DEFAULT_MIN_CUTOFF;
constexpr float EuroBeta = CONFIG::AIR_MOUSE::FILTER::DEFAULT_BETA;
constexpr float EuroDCutoff = CONFIG::AIR_MOUSE::FILTER::DEFAULT_D_CUTOFF;

namespace Jiggler {
constexpr uint8_t Mode = 0;  // MouseJigglerMode::JIGGLER_OFF
constexpr uint32_t IntervalS = CONFIG::AIR_MOUSE::JIGGLER::DEFAULT_INTERVAL_S;
constexpr uint16_t MoveDistance = CONFIG::AIR_MOUSE::JIGGLER::DEFAULT_MOVE_DISTANCE;
constexpr bool RandomInterval = CONFIG::AIR_MOUSE::JIGGLER::DEFAULT_RANDOM_INTERVAL;
}  // namespace Jiggler
}  // namespace AirMouse

namespace Imu {
constexpr bool UiMonitorEnabled = false;
constexpr bool AlarmMonitorEnabled = false;
constexpr float BaselineX = 0.0f;
constexpr float BaselineY = 0.0f;
constexpr float BaselineZ = 1.0f;
constexpr float TiltThresholdDeg = 30.0f;
constexpr float TiltHysteresisDeg = 5.0f;
constexpr uint16_t TiltHoldMs = 750;
constexpr uint16_t TiltClearHoldMs = 1500;
constexpr float AccelDeltaThresholdG = 0.35f;
}  // namespace Imu

namespace Matrix {
constexpr uint8_t Brightness = UI::MATRIX::BRIGHTNESS_DEFAULT;
constexpr uint8_t AlarmMode = 1;  // MatrixAlarmMode::ICON
constexpr uint8_t Rotation = UI::MATRIX::DEFAULT_ROTATION;
constexpr bool AutoRotate = UI::MATRIX::DEFAULT_AUTO_ROTATE;
constexpr bool EffectEnabled = UI::MATRIX::DEFAULT_EFFECT_ENABLED;
constexpr uint8_t EffectEngine = UI::MATRIX::DEFAULT_EFFECT_ENGINE;
constexpr uint8_t EffectMode = UI::MATRIX::DEFAULT_EFFECT_MODE;
constexpr uint32_t EffectSpeed = UI::MATRIX::DEFAULT_EFFECT_SPEED;
constexpr uint32_t EffectColor = UI::MATRIX::DEFAULT_EFFECT_COLOR_PRIMARY;
constexpr uint32_t EffectColor2 = UI::MATRIX::DEFAULT_EFFECT_COLOR_SECONDARY;
constexpr uint32_t EffectColor3 = UI::MATRIX::DEFAULT_EFFECT_COLOR_TERTIARY;
constexpr uint8_t EffectReactivityProvider = UI::MATRIX::DEFAULT_EFFECT_REACTIVITY_PROVIDER;
constexpr uint8_t EffectReactivityGain = UI::MATRIX::DEFAULT_EFFECT_REACTIVITY_GAIN;
constexpr uint8_t BackgroundMode = UI::MATRIX::DEFAULT_BACKGROUND_MODE;
constexpr bool DataVisualizationEnabled = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_ENABLED;
constexpr uint8_t DataVisualizationSource = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_SOURCE;
constexpr uint8_t DataVisualizationMetric = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_METRIC;
constexpr uint8_t DataVisualizationMode = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_MODE;
constexpr float DataVisualizationMin = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_MIN;
constexpr float DataVisualizationMax = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_MAX;
constexpr uint32_t DataVisualizationColorMin = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_COLOR_MIN;
constexpr uint32_t DataVisualizationColorMid = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_COLOR_MID;
constexpr uint32_t DataVisualizationColorMax = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_COLOR_MAX;
constexpr uint8_t DataVisualizationBrightnessMin = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_BRIGHTNESS_MIN;
constexpr uint8_t DataVisualizationBrightnessMax = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_BRIGHTNESS_MAX;
constexpr uint8_t DataVisualizationSmoothing = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_SMOOTHING;
constexpr uint8_t DataVisualizationStaleBehavior = UI::MATRIX::DEFAULT_DATA_VISUALIZATION_STALE_BEHAVIOR;

namespace Menu {
constexpr uint32_t TextColor = UI::MATRIX::MENU_TEXT_COLOR_DEFAULT;
constexpr uint16_t ScrollSpeed = UI::MATRIX::SCROLL_INTERVAL_MS;
constexpr bool Enabled = UI::MATRIX::MENU_ENABLED_DEFAULT;
}  // namespace Menu
}  // namespace Matrix

namespace Macro {
constexpr bool Enabled = CONFIG::MACRO::DEFAULT_ENABLED;
constexpr uint32_t BootDelayMs = CONFIG::MACRO::DEFAULT_BOOT_DELAY_MS;
}  // namespace Macro

namespace Keyboard {
constexpr bool Enabled = false;
}  // namespace Keyboard

namespace Compensation {
constexpr bool Enabled = FACTORY_SCD41_OFFSET_ENABLED;
constexpr float BaseTempOffset = FACTORY_SCD41_BASE_OFFSET;
constexpr float ReferenceCpuTemp = FACTORY_SCD41_REF_CPU_TEMP;
constexpr float TempOffsetPerCpuDegree = FACTORY_SCD41_OFFSET_SLOPE;
constexpr float MinTempOffset = FACTORY_SCD41_MIN_OFFSET;
constexpr float MaxTempOffset = FACTORY_SCD41_MAX_OFFSET;
}  // namespace Compensation

namespace UsbTerminal {
constexpr bool Enabled = TIMEOUT::USB_TERMINAL::DEFAULT_ENABLED;
constexpr uint32_t IdleTimeoutMs = TIMEOUT::USB_TERMINAL::DEFAULT_IDLE_TIMEOUT_MS;
constexpr const char* TargetPort = "auto";
}  // namespace UsbTerminal

}  // namespace RTC::Defaults
