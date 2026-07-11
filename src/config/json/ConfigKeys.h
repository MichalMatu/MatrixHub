#pragma once

#include <Arduino.h>
#include <cstdint>

namespace CONFIG {
namespace Keys {
    constexpr const char* kNotification = "notification";
    constexpr const char* kWifiSensing = "wifiSensing";
    constexpr const char* kBle = "ble";
    constexpr const char* kShelly = "shelly";
    constexpr const char* kAlarms = "alarms";
    constexpr const char* kHeartbeat = "heartbeat";
    constexpr const char* kMode = "mode";
    constexpr const char* kTelegramEnabled = "telegram_enabled";
    constexpr const char* kWebhookEnabled = "webhook_enabled";
    constexpr const char* kBotToken = "bot_token";
    constexpr const char* kChatId = "chat_id";
    constexpr const char* kCommandsEnabled = "commands_enabled";
    constexpr const char* kWebhookUrl = "webhook_url";
    constexpr const char* kPushoverEnabled = "pushover_enabled";
    constexpr const char* kPushoverUser = "pushover_user";
    constexpr const char* kPushoverToken = "pushover_token";
    constexpr const char* kEnabled = "enabled";
    constexpr const char* kSampleIntervalMs = "sample_interval_ms";
    constexpr const char* kVarianceThreshold = "variance_threshold";
    constexpr const char* kCsiAlarm = "csi_alarm";
    constexpr const char* kBands = "bands";
    constexpr const char* kStart = "start";
    constexpr const char* kEnd = "end";
    constexpr const char* kBaselineFrames = "baseline_frames";
    constexpr const char* kTopK = "top_k";
    constexpr const char* kEnterThreshold = "enter_threshold";
    constexpr const char* kClearThreshold = "clear_threshold";
    constexpr const char* kHoldMs = "hold_ms";
    constexpr const char* kClearHoldMs = "clear_hold_ms";
    constexpr const char* kMinNoise = "min_noise";
    constexpr const char* kMinEnergy = "min_energy";
    constexpr const char* kNoisyThreshold = "noisy_threshold";
    constexpr const char* kAutoRecalibration = "auto_recalibration";
    constexpr const char* kSensitivity = "sensitivity";
    constexpr const char* kSensors = "sensors";
    constexpr const char* kMac = "mac";
    constexpr const char* kAlias = "alias";
    constexpr const char* kScannerActive = "scanner_active";
    constexpr const char* kSaved = "saved";
    constexpr const char* kLastSeen = "last_seen";
    constexpr const char* kRssi = "rssi";
    constexpr const char* kBatt = "batt";
    constexpr const char* kTemp = "temp";
    constexpr const char* kHumid = "humid";
    constexpr const char* kCo2 = "co2"; // Added kCo2
    constexpr const char* kDevices = "devices";
    constexpr const char* kName = "name";
    constexpr const char* kIp = "ip";
    constexpr const char* kId = "id";
    constexpr const char* kRelayIndex = "relay_index";
    constexpr const char* kRules = "rules";
    constexpr const char* kSource = "source";
    constexpr const char* kCreatedAt = "created_at";
    constexpr const char* kUpdatedAt = "updated_at";
    constexpr const char* kChangedAt = "changed_at";
    constexpr const char* kSampledAt = "sampled_at";
    constexpr const char* kBleDeviceMac = "ble_device_mac";
    constexpr const char* kGpioId = "gpio_id";
    constexpr const char* kOperator = "operator";
    constexpr const char* kThreshold = "threshold";
    constexpr const char* kSeverity = "severity";
    constexpr const char* kNotifyChannels = "notify_channels";
    constexpr const char* kCooldownSeconds = "cooldown_seconds";
    constexpr const char* kShellyDevices = "shelly_devices";
    constexpr const char* kSlots = "slots";
    constexpr const char* kUrl = "url";
    constexpr const char* kAllowInsecure = "allow_insecure";
    constexpr const char* kIntervalMs = "interval_ms";

    // Alarms
    constexpr const char* kSchemaVersion = "schema_version";
    constexpr const char* kShellyDeviceIds = "shelly_device_ids";
    constexpr const char* kGeneration = "generation";
    constexpr const char* kIsOn = "isOn";
    constexpr const char* kIsOnline = "isOnline";
    constexpr const char* kLastUpdate = "lastUpdate";
    constexpr const char* kVoltage = "voltage";
    constexpr const char* kCurrent = "current";
    constexpr const char* kEnergy = "energy";
    constexpr const char* kUdpPusher = "udp_pusher";
    constexpr const char* kHost = "host";
    constexpr const char* kPort = "port";
    constexpr const char* kFormat = "format";
    // Air Mouse
    constexpr const char* kAirMouse = "airmouse";
    constexpr const char* kSensitivityX = "sensitivity_x";
    constexpr const char* kSensitivityY = "sensitivity_y";
    constexpr const char* kDeadzone = "deadzone";
    
    // CSI (WiFi Sensing)
    constexpr const char* kWsCsi = "/ws/csi";
    constexpr const char* kCsiBroadcastName = "WS-CSI";
    constexpr const char* kWsCsiCaptureV1 = "/ws/csi-capture/v1";
    constexpr const char* kCsiCaptureBroadcastName = "WS-CSI-Capture";
    constexpr const char* kWsUsbTerminal = "/ws/usbterminal";
    constexpr const char* kUsbTerminalBroadcastName = "WS-UsbTerminal";
    
    constexpr const char* kShakeThresholdG = "shake_threshold_g";
    constexpr const char* kClickDebounceMs = "click_debounce_ms";
    constexpr const char* kMovementEnabled = "movement_enabled";
    constexpr const char* kClickEnabled = "click_enabled";
    constexpr const char* kRunning = "running";
    constexpr const char* kCalibrating = "calibrating";
    constexpr const char* kAccelerationEnabled = "acceleration_enabled";
    constexpr const char* kAccelerationFactor = "acceleration_factor";
    constexpr const char* kTapThresholdG = "tap_threshold_g";
    constexpr const char* kDoubleClickWindowMs = "double_click_window_ms";
    constexpr const char* kEuroMinCutoff = "euro_min_cutoff";
    constexpr const char* kEuroBeta = "euro_beta";
    constexpr const char* kEuroDCutoff = "euro_d_cutoff";
    constexpr const char* kJiggler = "jiggler";
    constexpr const char* kGyroOffsetX = "gyro_offset_x";
    constexpr const char* kGyroOffsetY = "gyro_offset_y";
    constexpr const char* kGyroOffsetZ = "gyro_offset_z";
    constexpr const char* kLastDeltaG = "last_delta_g";
    constexpr const char* kImu = "imu";
    constexpr const char* kGx = "gx";
    constexpr const char* kGy = "gy";
    constexpr const char* kGz = "gz";
    constexpr const char* kAx = "ax";
    constexpr const char* kAy = "ay";
    constexpr const char* kAz = "az";
    constexpr const char* kX = "x";
    constexpr const char* kY = "y";
    constexpr const char* kZ = "z";
    constexpr const char* kDesiredConsumers = "desired_consumers";
    constexpr const char* kRunningConsumers = "running_consumers";
    constexpr const char* kDesiredMask = "desired_mask";
    constexpr const char* kRunningMask = "running_mask";
    constexpr const char* kInitialized = "initialized";
    constexpr const char* kTransitionInProgress = "transition_in_progress";
    constexpr const char* kSampleFresh = "sample_fresh";
    constexpr const char* kSampleAgeMs = "sample_age_ms";
    constexpr const char* kLastSampleMs = "last_sample_ms";
    constexpr const char* kLastStartAttemptMs = "last_start_attempt_ms";
    constexpr const char* kNextRetryMs = "next_retry_ms";
    constexpr const char* kLastStartDurationMs = "last_start_duration_ms";
    constexpr const char* kLastStartError = "last_start_error";
    constexpr const char* kRetryPending = "retry_pending";
    constexpr const char* kUiMonitorEnabled = "ui_monitor_enabled";
    constexpr const char* kAlarmMonitorEnabled = "alarm_monitor_enabled";
    constexpr const char* kOrientationBaselineValid = "orientation_baseline_valid";
    constexpr const char* kOrientationBaseline = "orientation_baseline";
    constexpr const char* kBaselineCalibratedAt = "baseline_calibrated_at";
    constexpr const char* kCalibrationRevision = "calibration_revision";
    constexpr const char* kTiltThresholdDeg = "tilt_threshold_deg";
    constexpr const char* kTiltHysteresisDeg = "tilt_hysteresis_deg";
    constexpr const char* kTiltHoldMs = "tilt_hold_ms";
    constexpr const char* kTiltClearHoldMs = "tilt_clear_hold_ms";
    constexpr const char* kAccelDeltaThresholdG = "accel_delta_threshold_g";
    constexpr const char* kAccelMagnitudeG = "accel_magnitude_g";
    constexpr const char* kAccelDeltaG = "accel_delta_g";
    constexpr const char* kGyroMagnitudeDps = "gyro_magnitude_dps";
    constexpr const char* kTiltDeg = "tilt_deg";
    constexpr const char* kBaselineValid = "baseline_valid";
    constexpr const char* kAlarm = "alarm";
    constexpr const char* kTriggered = "triggered";
    constexpr const char* kPendingTrigger = "pending_trigger";
    constexpr const char* kPendingClear = "pending_clear";
    constexpr const char* kReason = "reason";
    constexpr const char* kTriggerValue = "trigger_value";
    constexpr const char* kTriggerHoldElapsedMs = "trigger_hold_elapsed_ms";
    constexpr const char* kClearHoldElapsedMs = "clear_hold_elapsed_ms";
    constexpr const char* kConsumers = "consumers";
    constexpr const char* kMetrics = "metrics";
    constexpr const char* kSample = "sample";
    // kMode already defined above
    constexpr const char* kInterval = "interval";
    constexpr const char* kMoveDistance = "move_distance";
    constexpr const char* kRandomInterval = "random_interval";
    // Click source & actions
    constexpr const char* kClickSource = "click_source";
    constexpr const char* kSingleClickAction = "single_click_action";
    constexpr const char* kDoubleClickAction = "double_click_action";
    constexpr const char* kTripleClickAction = "triple_click_action";
    constexpr const char* kSingleClickScript = "single_click_script";
    constexpr const char* kDoubleClickScript = "double_click_script";
    constexpr const char* kTripleClickScript = "triple_click_script";
    // Matrix
    constexpr const char* kMatrix = "matrix";
    constexpr const char* kBrightness = "brightness";
    constexpr const char* kAlarmMode = "alarm_mode";
    constexpr const char* kRotation = "rotation";
    constexpr const char* kAutoRotate = "auto_rotate";
    constexpr const char* kEffectEnabled = "effect_enabled";
    constexpr const char* kEffectEngine = "effect_engine";
    constexpr const char* kEffectMode = "effect_mode";
    constexpr const char* kEffectSpeed = "effect_speed";
    constexpr const char* kEffectColor = "effect_color";
    constexpr const char* kEffectColor2 = "effect_color_2";
    constexpr const char* kEffectColor3 = "effect_color_3";
    constexpr const char* kEffectReactivityProvider = "effect_reactivity_provider";
    constexpr const char* kEffectReactivityGain = "effect_reactivity_gain";
    constexpr const char* kBackgroundMode = "background_mode";
    constexpr const char* kDataVisualizationEnabled = "data_visualization_enabled";
    constexpr const char* kDataVisualizationSource = "data_visualization_source";
    constexpr const char* kDataVisualizationMetric = "data_visualization_metric";
    constexpr const char* kDataVisualizationMode = "data_visualization_mode";
    constexpr const char* kDataVisualizationMin = "data_visualization_min";
    constexpr const char* kDataVisualizationMax = "data_visualization_max";
    constexpr const char* kDataVisualizationColorMin = "data_visualization_color_min";
    constexpr const char* kDataVisualizationColorMid = "data_visualization_color_mid";
    constexpr const char* kDataVisualizationColorMax = "data_visualization_color_max";
    constexpr const char* kDataVisualizationBrightnessMin = "data_visualization_brightness_min";
    constexpr const char* kDataVisualizationBrightnessMax = "data_visualization_brightness_max";
    constexpr const char* kDataVisualizationSmoothing = "data_visualization_smoothing";
    constexpr const char* kDataVisualizationStaleBehavior = "data_visualization_stale_behavior";
    constexpr const char* kDataVisualizationDeviceId = "data_visualization_device_id";
    constexpr const char* kMenuTextColor = "menu_text_color";
    constexpr const char* kMenuScrollSpeed = "menu_scroll_speed";
    constexpr const char* kMenuEnabled = "menu_enabled";
    constexpr const char* kCustomIcons = "custom_icons";

    // Macros
    constexpr const char* kMacros = "macros";
    constexpr const char* kBootDelay = "boot_delay";
    constexpr const char* kBootScript = "boot_script";

    // Keyboard
    constexpr const char* kKeyboard = "keyboard";

    // USB Terminal
    constexpr const char* kUsbTerminal = "usb_terminal";
    constexpr const char* kTargetPort = "target_port";
    constexpr const char* kIdleTimeoutMs = "idle_timeout_ms";

    // GPIO
    constexpr const char* kGpio = "gpio";
    constexpr const char* kChannels = "channels";
    constexpr const char* kPin = "pin";
    constexpr const char* kPull = "pull";
    constexpr const char* kInverted = "inverted";
    constexpr const char* kDebounceMs = "debounce_ms";
    constexpr const char* kInitialOutput = "initial_output";
    constexpr const char* kRaw = "raw";
    constexpr const char* kLogical = "logical";
    constexpr const char* kStable = "stable";
    constexpr const char* kConfigured = "configured";
    constexpr const char* kAllowed = "allowed";
    constexpr const char* kValue = "value";

    // Keyboard
    constexpr const char* kText = "text";
    constexpr const char* kEnter = "enter";
    constexpr const char* kKey = "key";
    constexpr const char* kKeys = "keys";

    // Macro status/file
    constexpr const char* kFilename = "filename";
    constexpr const char* kContent = "content";
    constexpr const char* kCurrentScript = "current_script";
    constexpr const char* kStatus = "status";
    constexpr const char* kCurrentLine = "current_line";
    constexpr const char* kUptimeMs = "uptime_ms";
    constexpr const char* kLastError = "last_error";
    
    // Logging (New JSON keys)
    constexpr const char* kLogging = "logging";
    constexpr const char* kLevel = "level";
    
    // Power (New JSON keys)
    constexpr const char* kPower = "power";
    constexpr const char* kWakeReason = "wake_reason";
    constexpr const char* kWakeCauseRaw = "wake_cause_raw";
    constexpr const char* kWakeGpioMask = "wake_gpio_mask";
    constexpr const char* kWakeExt1Mask = "wake_ext1_mask";
    constexpr const char* kSleepRequested = "sleep_requested";
    constexpr const char* kSleepEtaMs = "sleep_eta_ms";
    constexpr const char* kWakeIntervalMs = "wake_interval_ms";
    constexpr const char* kWakeTimerEnabled = "wake_timer_enabled";
    constexpr const char* kWakeButtonEnabled = "wake_button_enabled";
    constexpr const char* kWakeTouchEnabled = "wake_touch_enabled";
    constexpr const char* kWakeTouchGpio = "wake_touch_gpio";
    constexpr const char* kWakeTouchThreshold = "wake_touch_threshold";
    constexpr const char* kTimerWakeAwakeMs = "timer_wake_awake_ms";
    constexpr const char* kButtonWakeAwakeMs = "button_wake_awake_ms";
    constexpr const char* kWakeAwakeWindowMs = "wake_awake_window_ms";
    constexpr const char* kWakeAwakeEtaMs = "wake_awake_eta_ms";
    constexpr const char* kLastActivityMs = "last_activity_ms";
    constexpr const char* kThermalState = "thermal_state";
    constexpr const char* kThermalTempC = "thermal_temp_c";
    constexpr const char* kThermalCpuMhz = "thermal_cpu_mhz";
    constexpr const char* kThermalThrottled = "thermal_throttled";
    constexpr const char* kThermalMatrixLimit = "thermal_matrix_limit";
    constexpr const char* kThermalSoftC = "thermal_soft_c";
    constexpr const char* kThermalHardC = "thermal_hard_c";
    constexpr const char* kThermalCriticalC = "thermal_critical_c";

    // System Info (Read-Only)
    constexpr const char* kEspPlatform = "esp_platform";
    constexpr const char* kFirmwareVersion = "firmware_version";
    constexpr const char* kFirmwareName = "firmware_name";
    constexpr const char* kFirmwareBuiltTarget = "firmware_built_target";
    constexpr const char* kFirmwareCommit = "firmware_commit";
    constexpr const char* kFirmwareDirty = "firmware_dirty";
    constexpr const char* kCpuFreqMhz = "cpu_freq_mhz";
    constexpr const char* kCpuType = "cpu_type";
    constexpr const char* kCpuRev = "cpu_rev";
    constexpr const char* kCpuCores = "cpu_cores";
    constexpr const char* kFlashChipSize = "flash_chip_size";
    constexpr const char* kFlashChipSpeed = "flash_chip_speed";
    constexpr const char* kSketchSize = "sketch_size";
    constexpr const char* kFreeSketchSpace = "free_sketch_space";
    constexpr const char* kMacAddress = "mac_address";
    constexpr const char* kSdkVersion = "sdk_version";
    constexpr const char* kArduinoVersion = "arduino_version";
    constexpr const char* kCpuResetReason = "cpu_reset_reason";
    constexpr const char* kCoreTemp = "core_temp";
    constexpr const char* kFreeHeap = "free_heap";
    constexpr const char* kTotalHeap = "total_heap";
    constexpr const char* kUsedHeap = "used_heap";
    constexpr const char* kMinFreeHeap = "min_free_heap";
    constexpr const char* kMaxAllocHeap = "max_alloc_heap";
    constexpr const char* kPsramSize = "psram_size";
    constexpr const char* kFreePsram = "free_psram";
    constexpr const char* kUsedPsram = "used_psram";
    constexpr const char* kFsTotal = "fs_total";
    constexpr const char* kFsUsed = "fs_used";
    constexpr const char* kUptime = "uptime";
    constexpr const char* kCompileDate = "compile_date";
    constexpr const char* kCompileTime = "compile_time";
    constexpr const char* kInactivityTimeoutMs = "inactivity_timeout_ms";
    constexpr const char* kGraceAfterBootMs = "grace_after_boot_ms";
    constexpr const char* kSleepEnabled = "sleep_enabled";

    // SCD4x Compensation
    constexpr const char* kCompensation = "compensation";
    constexpr const char* kBaseTempOffset = "base_temp_offset";
    constexpr const char* kReferenceCpuTemp = "reference_cpu_temp";
    constexpr const char* kTempOffsetPerCpuDegree = "temp_offset_per_cpu_degree";
    constexpr const char* kMinTempOffset = "min_temp_offset";
    constexpr const char* kMaxTempOffset = "max_temp_offset";

    // File Manager
    constexpr const char* kFmFiles = "files";
    constexpr const char* kFmOk = "ok";
    constexpr const char* kFmSize = "size";
    constexpr const char* kFmDirectory = "directory";
    constexpr const char* kFmListPath = "/rest/fs/list";
    constexpr const char* kFmDownloadPath = "/rest/fs/download";
    constexpr const char* kFmUploadPath = "/rest/fs/upload";
    constexpr const char* kFmParamDir = "dir";
    constexpr const char* kFmParamPath = "path";
    constexpr const char* kFmRemovePath = "/rest/fs/remove";
    
    namespace FILEMGR {
        constexpr size_t kUploadMaxFileSize = 10 * 1024 * 1024; // 10 MB default sanity limit
        constexpr size_t kUploadMinFreeSpace = 32 * 1024;       // 32 KB minimum free space required
    }

    // Framework
    namespace FRAMEWORK {
        constexpr uint32_t TASK_PRIORITY = 5; // Standard FreeRTOS priority
        constexpr uint32_t MAX_URI_HANDLERS = 255;
        constexpr bool USE_JWT_AUTH = false;
    }

}
}
