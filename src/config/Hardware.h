/**
 * @file Hardware.h
 * @brief Hardware Configuration: Pins, I2C, Power, Thermal
 */

#pragma once

#ifndef UNIT_TEST
#include <Arduino.h>
#endif
#include <cstdint>
#include "App.h"

// ============================================================================
// Hardware Pin Definitions (from Hardware.h)
// ============================================================================
namespace HW {
    // Sentinel value meaning: feature/pin not wired on this board.
    constexpr uint8_t PIN_DISABLED = 0xFF;

    // Waveshare ESP32-S3-Matrix: BOOT button is on GPIO0 (active LOW).
    constexpr uint8_t USER_BUTTON = 0;
    constexpr bool USER_BUTTON_ACTIVE_LOW = true;

    constexpr uint8_t LED_PIN = PIN_DISABLED; // Matrix board uses matrix for status
}

namespace MATRIX {
    constexpr uint8_t PIN = 14;
    constexpr uint8_t WIDTH = 8;
    constexpr uint8_t HEIGHT = 8;
}

namespace IMU {
    constexpr uint8_t SDA = 11;
    constexpr uint8_t SCL = 12;
    constexpr uint8_t INT1 = 8;
    constexpr uint8_t ADDRESS = 0x6B; // Common address for QMI8658
}

namespace I2C {
    // Unified I2C Bus (Matches IMU pins on S3 Matrix)
    constexpr uint8_t SCL_PIN = 12;
    constexpr uint8_t SDA_PIN = 11;
}

namespace I2C_SCD4X {
    // Secondary I2C Bus for SCD41 (Wire1)
    // SCL=7, SDA=6 (User requested specific layout for soldering)
    constexpr uint8_t SCL_PIN = 7;
    constexpr uint8_t SDA_PIN = 6;
    constexpr uint8_t I2C_ADDRESS = 0x62;
}

namespace COM {
    constexpr uint32_t SERIAL_BAUD_RATE = 115200;
}

// ============================================================================
// Power Management (from Hardware.h)
// ============================================================================
namespace POWER {
    // Default Runtime Behavior
    constexpr uint32_t INACTIVITY_TIMEOUT_MS = 300000;   // 5min default
    constexpr uint32_t GRACE_AFTER_BOOT_MS = 60000;      // 1min default
    constexpr uint32_t WAKE_INTERVAL_MS = FACTORY_POWER_WAKE_INTERVAL_MS;        // 10 minutes
    constexpr uint32_t TIMER_WAKE_AWAKE_MS = FACTORY_POWER_TIMER_WAKE_AWAKE_MS;  // 1 minute
    constexpr uint32_t BUTTON_WAKE_AWAKE_MS = FACTORY_POWER_BUTTON_WAKE_AWAKE_MS; // 2 minutes
    constexpr uint8_t TOUCH_WAKE_GPIO = FACTORY_POWER_TOUCH_WAKE_GPIO;
    constexpr uint32_t TOUCH_WAKE_THRESHOLD = FACTORY_POWER_TOUCH_WAKE_THRESHOLD;
    
    // API Validation Ranges
    constexpr uint32_t INACTIVITY_TIMEOUT_MIN_MS = 300000;    // 5min
    constexpr uint32_t INACTIVITY_TIMEOUT_MAX_MS = 86400000;  // 24h
    constexpr uint32_t GRACE_MIN_MS = 60000;                 // 1min
    constexpr uint32_t GRACE_MAX_MS = 600000;                 // 10min
    constexpr uint32_t WAKE_INTERVAL_MIN_MS = 30000;          // 30s
    constexpr uint32_t WAKE_INTERVAL_MAX_MS = 86400000;       // 24h
    constexpr uint32_t TIMER_WAKE_AWAKE_MIN_MS = 30000;       // 30s
    constexpr uint32_t TIMER_WAKE_AWAKE_MAX_MS = 600000;      // 10min
    constexpr uint32_t BUTTON_WAKE_AWAKE_MIN_MS = 60000;      // 1min
    constexpr uint32_t BUTTON_WAKE_AWAKE_MAX_MS = 900000;     // 15min
    constexpr uint8_t TOUCH_WAKE_GPIO_MIN = 1;
    constexpr uint8_t TOUCH_WAKE_GPIO_MAX = 7;
    constexpr uint32_t TOUCH_WAKE_THRESHOLD_MIN = 100;
    constexpr uint32_t TOUCH_WAKE_THRESHOLD_MAX = 100000;
    
    // Sleep Transition & Logging
    constexpr uint32_t COUNTDOWN_LOG_INTERVAL_MS = 10000;    // 10s
    constexpr uint32_t POST_WAKE_LOG_INTERVAL_MS = 10000;    // 10s
    constexpr uint32_t SLEEP_ENTRY_FAILSAFE_MS = 30000;      // 30s
}

// ============================================================================
// Thermal Configuration (from Hardware.h)
// ============================================================================
namespace THERMAL {
    // Temperature Thresholds (°C)
    constexpr float TEMP_SOFT_THROTTLE = 60.0f;
    constexpr float TEMP_HARD_THROTTLE = 68.0f;
    constexpr float TEMP_CRITICAL = 80.0f;
    constexpr float HYSTERESIS = 5.0f;
    
    constexpr uint32_t SHUTDOWN_COOLING_MS = 30000; // 30s deep sleep for cooling

    // CPU Frequencies (MHz)
    constexpr uint32_t FREQ_NORMAL = 240;  // Full speed
    constexpr uint32_t FREQ_SOFT   = 160;  // Reduced
    constexpr uint32_t FREQ_HARD   = 160;  // Safe minimum (stacks in DRAM, no cache risk)

    // Task Configuration
    constexpr uint32_t MONITOR_INTERVAL_MS = 5000;
}

// ============================================================================
// Factory Defaults: SCD41 Compensation
// ============================================================================
#ifndef FACTORY_SCD41_OFFSET_ENABLED
#define FACTORY_SCD41_OFFSET_ENABLED false
#endif

#ifndef FACTORY_SCD41_BASE_OFFSET
#define FACTORY_SCD41_BASE_OFFSET 2.0f
#endif

#ifndef FACTORY_SCD41_REF_CPU_TEMP
#define FACTORY_SCD41_REF_CPU_TEMP 45.0f
#endif

#ifndef FACTORY_SCD41_OFFSET_SLOPE
#define FACTORY_SCD41_OFFSET_SLOPE 0.2f
#endif

#ifndef FACTORY_SCD41_MIN_OFFSET
#define FACTORY_SCD41_MIN_OFFSET 0.0f
#endif

#ifndef FACTORY_SCD41_MAX_OFFSET
#define FACTORY_SCD41_MAX_OFFSET 8.0f
#endif

// --- Merged from src/config/HardwareConfig.h ---
namespace SENSOR {
    namespace SCD4X {
        constexpr uint32_t POWER_UP_DELAY_MS = 1000;
        constexpr uint32_t STOP_PERIODIC_DELAY_MS = 500;
        constexpr uint32_t REINIT_DELAY_MS = 30;

        constexpr uint16_t CO2_MIN_PPM = 400;
        constexpr uint16_t CO2_MAX_PPM = 10000;
        constexpr float TEMP_MIN_C = -20.0f;
        constexpr float TEMP_MAX_C = 70.0f;
        constexpr float HUMID_MIN_PCT = 0.0f;
        constexpr float HUMID_MAX_PCT = 100.0f;
    }
    namespace TASK_LOOP {
        constexpr uint32_t STABILIZATION_TIMEOUT_MS = 7000;
        constexpr uint32_t STABILIZATION_POLL_MS = 1000;
        constexpr uint8_t SELF_HEAL_FAILURE_THRESHOLD = 10;
        constexpr uint32_t SELF_HEAL_BACKOFF_MS = 5000;
        constexpr uint32_t SELF_HEAL_BACKOFF_STEP_MS = 1000;
        constexpr uint32_t READ_ERROR_LOG_INTERVAL_MS = 60000;
        constexpr uint32_t SELF_HEAL_LOG_INTERVAL_MS = 300000;
        constexpr uint32_t UPDATE_CALLBACK_LOCK_TIMEOUT_MS = 5;
        constexpr uint32_t POLL_STEP_MS = 1000;
    }
    namespace LOG_BUFFER {
        constexpr uint32_t IO_LOCK_TIMEOUT_MS = 10;
        constexpr uint32_t STATUS_LOCK_TIMEOUT_MS = 50;
    }
    namespace BLE {
        constexpr uint32_t SCAN_DURATION_SEC = 5;       // Increased from 2s to reduce restart overhead
        constexpr uint32_t SCAN_INTERVAL_MS = 500;      // 500ms interval
        constexpr uint32_t SCAN_WINDOW_MS = 450;        // 450ms window (90% duty cycle) for better reception
        
        // Turbo Discovery Mode (Manual search)
        constexpr uint32_t DISCOVERY_SCAN_INTERVAL_MS = 200; // Fast cycle
        constexpr uint32_t DISCOVERY_SCAN_WINDOW_MS = 200;   // 100% duty cycle
    }
    namespace WIFI_SENSING {
        constexpr uint32_t RETRY_DELAY_DISCONNECTED_MS = 2000;
        constexpr uint32_t RETRY_DELAY_AP_MODE_MS = 5000;
        constexpr uint32_t CSI_PING_INTERVAL_MS = 100;   // 10 Hz (Aligned with Frontend)
        constexpr uint32_t CSI_FRONTEND_STOP_GRACE_MS = 1500;
    }
    constexpr uint32_t SNAPSHOT_TIMEOUT_MS = 300000;
}
namespace DATALOG {
    constexpr size_t MIN_FREE_SPACE_BYTES = 50 * 1024;
    constexpr uint32_t ROTATE_DELETE_PAUSE_MS = 10;
}
namespace LOG_CFG {
    // Ring buffer
    constexpr size_t RING_BUFFER_CAPACITY = 500;
    constexpr uint32_t RING_APPEND_LOCK_MS = 10;
    constexpr uint32_t RING_SNAPSHOT_LOCK_MS = 50;

    // USB output
    constexpr size_t USB_BOOT_LINE_BUF_BYTES = 192;
    constexpr size_t USB_VPRINTF_BUF_BYTES = 256;
    constexpr uint16_t USB_BOOT_REPLAY_MAX_LINES = 200;
    constexpr uint16_t USB_BOOT_REPLAY_CHUNK_LINES = 32;
    constexpr uint32_t USB_ENUM_DELAY_MS = 100;

    // /rest/logs/tail JSON streaming buffers
    constexpr size_t LOG_TAIL_JSON_BUF_BYTES = 1024;    // Increased temporary buffer to format batch lines
    constexpr size_t LOG_TAIL_ESCAPED_TAG_BYTES = 64;
    constexpr size_t LOG_TAIL_ESCAPED_MSG_BYTES = 256;  // Reduced from 448 since max msg is 96
    constexpr uint16_t LOG_TAIL_COPY_CHUNK_LINES = 64;  // Increase window size for faster lookup
    constexpr size_t LOG_TAIL_OUT_CHUNK_BYTES = 2048;   // Network transmission chunk buffer (PSRAM)

    // /api/logs directory scanning
    // These values intentionally trade a little scan latency for much lower
    // scheduler/mutex overhead than the old "yield after every entry" model.
    // TODO: If boards accumulate many months of logs, prefer a cached index or
    // month-on-demand API over increasing this batch indefinitely.
    constexpr size_t LOG_FILE_LIST_SCAN_BATCH_ENTRIES = 32;
    constexpr uint32_t LOG_FILE_LIST_YIELD_MS = 1;
    // Short-lived cache for repeated opens of the Logs page. This is a passive
    // TTL checked on the next request, not a background timer that clears the
    // cache by itself. Keep it small enough that newly created log files become
    // visible quickly even though only DELETE invalidates the cache explicitly.
    constexpr uint32_t LOG_FILE_LIST_CACHE_TTL_MS = 5000;
}
namespace ALARM {
    constexpr uint32_t EVAL_INTERVAL_MS = 10000;  // 10s - matches sensor update rates
    constexpr uint32_t LED_LATCH_REFRESH_MS = 5000;
}

namespace CONFIG {
namespace COMPENSATION {
    constexpr float CPU_TEMP_EMA_ALPHA = 0.05f;
    constexpr float CPU_TEMP_FALLBACK_C = 40.0f;
    constexpr uint8_t LOG_INTERVAL_TICKS = 60;
    constexpr float MAGNUS_A = 17.62f;
    constexpr float MAGNUS_B = 243.12f;
    constexpr float MAGNUS_DENOM_MIN = 0.01f;
    // Fast-path threshold for humidity compensation (skip expf when delta is negligible).
    constexpr float HUMID_FAST_PATH_THRESHOLD = 0.01f;
}
}

namespace LIMITS {
    namespace ALARMS {
        constexpr uint32_t MIN_COOLDOWN_SEC = 10;
        constexpr uint32_t MAX_COOLDOWN_SEC = 86400;
        constexpr float MIN_THRESHOLD = -100.0f;
        constexpr float MAX_THRESHOLD = 20000.0f;
    }
    namespace COMPENSATION {
        constexpr float MIN_BASE_OFFSET    = -5.0f;
        constexpr float MAX_BASE_OFFSET    = 20.0f;
        constexpr float MIN_REF_CPU_TEMP   = 20.0f;
        constexpr float MAX_REF_CPU_TEMP   = 80.0f;
        constexpr float MIN_SLOPE          = 0.0f;
        constexpr float MAX_SLOPE          = 2.0f;
        constexpr float MIN_OFFSET_CLAMP   = -10.0f;
        constexpr float MAX_OFFSET_CLAMP   = 25.0f;
    }
}
