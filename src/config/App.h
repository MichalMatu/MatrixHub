/**
 * @file App.h
 * @brief Application Configuration: Logging, Notifications, JSON Keys, Factory Defaults
 */

#pragma once

#include <Arduino.h>
#include <esp_log.h>

#ifndef FACTORY_POWER_WAKE_TIMER_ENABLED
#define FACTORY_POWER_WAKE_TIMER_ENABLED true
#endif

#ifndef FACTORY_POWER_WAKE_BUTTON_ENABLED
#define FACTORY_POWER_WAKE_BUTTON_ENABLED true
#endif

#ifndef FACTORY_POWER_WAKE_TOUCH_ENABLED
#define FACTORY_POWER_WAKE_TOUCH_ENABLED false
#endif

#ifndef FACTORY_POWER_WAKE_INTERVAL_MS
#define FACTORY_POWER_WAKE_INTERVAL_MS 600000
#endif

#ifndef FACTORY_POWER_TIMER_WAKE_AWAKE_MS
#define FACTORY_POWER_TIMER_WAKE_AWAKE_MS 60000
#endif

#ifndef FACTORY_POWER_BUTTON_WAKE_AWAKE_MS
#define FACTORY_POWER_BUTTON_WAKE_AWAKE_MS 120000
#endif

#ifndef FACTORY_POWER_TOUCH_WAKE_GPIO
#define FACTORY_POWER_TOUCH_WAKE_GPIO 4
#endif

#ifndef FACTORY_POWER_TOUCH_WAKE_THRESHOLD
#define FACTORY_POWER_TOUCH_WAKE_THRESHOLD 2000
#endif

#include "TaskConfig.h"
#include "Hardware.h"
#include "Network.h"

// ============================================================================
// Logging Configuration (from App.h)
// ============================================================================
namespace LOG {
    struct Settings;
}

namespace LoggingConfig {
    LOG::Settings get();
    void begin();
    bool setLevel(esp_log_level_t level);
}

// ============================================================================
// Notification Settings (from App.h)
// ============================================================================
namespace APP {
namespace LOGGING {
    // Boot-time level before persisted settings are loaded. Keep only hard
    // failures visible until Phase 1 can apply the saved runtime level.
    constexpr esp_log_level_t STARTUP_LEVEL = ESP_LOG_ERROR;
}

namespace NOTIFICATIONS {
    constexpr size_t TELEGRAM_MAX_TEXT_LEN = 768;
    constexpr uint32_t TELEGRAM_TIME_WAIT_MS = 5000;
    constexpr uint32_t TELEGRAM_ONLINE_PROBE_TIMEOUT_MS = 2000;
    constexpr uint16_t TELEGRAM_ONLINE_TCP_PORT = 443;
    constexpr uint8_t TELEGRAM_ONLINE_DNS_ATTEMPTS = 2;
    constexpr uint8_t TELEGRAM_ONLINE_TCP_ATTEMPTS = 2;
    constexpr uint32_t TELEGRAM_ONLINE_DNS_RETRY_DELAY_MS = 200;
    constexpr uint32_t TELEGRAM_ONLINE_TCP_RETRY_DELAY_MS = 250;
    constexpr uint32_t TELEGRAM_ONLINE_TIME_POLL_DELAY_MS = 200;
    constexpr int TELEGRAM_VALID_YEAR_MIN = 2020;
    constexpr int TELEGRAM_VALID_YEAR_MAX = 2099;
    constexpr uint32_t TELEGRAM_HTTP_IO_TIMEOUT_MS = 5000;
    constexpr uint32_t TELEGRAM_TLS_CONNECT_TIMEOUT_MS = 3000;
    constexpr uint32_t TELEGRAM_TLS_HANDSHAKE_TIMEOUT_SEC = 3;
    constexpr uint32_t TELEGRAM_POLL_INTERVAL_MS = 5000;

    constexpr size_t TELEGRAM_BUFFER_SIZE_URL = 256;
    constexpr size_t TELEGRAM_BUFFER_SIZE_PAYLOAD = 1536;

    constexpr uint32_t TELEGRAM_MUTEX_TIMEOUT_MS = 3000;
    constexpr uint32_t TELEGRAM_MUTEX_LONG_TIMEOUT_MS = 6000;

    constexpr size_t TELEGRAM_TLS_MIN_HEAP = 20000;
    constexpr int TELEGRAM_TLS_WARMUP_ATTEMPTS = 2;
    constexpr uint32_t TELEGRAM_TLS_WARMUP_RETRY_DELAY_MS = 1000;

    // Telegram API host (shared by TlsManager & ApiExecutor)
    constexpr const char* TELEGRAM_API_HOST = "api.telegram.org";

    // Polling backoff
    constexpr uint32_t TELEGRAM_MAX_BACKOFF_MS = 300000;         // 5 minutes max
    constexpr uint32_t TELEGRAM_MAX_BACKOFF_EXPONENT = 5;        // 2^5 = 32x multiplier cap
    constexpr uint32_t TELEGRAM_POLL_JITTER_MS = 2000;           // Random jitter range

    // Polling tuning
    constexpr int TELEGRAM_LONG_POLL_TIMEOUT_SEC = 1;            // Capped for mutex fairness
    constexpr size_t TELEGRAM_MAX_UPDATES_PER_POLL = 5;          // Must match ApiExecutor limit
    constexpr uint32_t TELEGRAM_NETWORK_SLOW_THRESHOLD_MS = 5000;
    constexpr uint32_t TELEGRAM_HEAP_DELTA_THRESHOLD = 2048;     // Log heap changes > this

    // Queue processor
    constexpr uint32_t TELEGRAM_MUTEX_WARNING_THRESHOLD_MS = 3000;
    constexpr uint32_t TELEGRAM_QUEUE_SEND_DELAY_MS = 3000;      // Breathing room between queued sends

    // Connection validator
    constexpr uint32_t TELEGRAM_ONLINE_CACHE_TTL_MS = 30000;     // 30s cache for online checks
}
}

// ============================================================================
// Sensor Task Timing (from App.h)
// ============================================================================
namespace SENSOR {
    constexpr uint32_t STACK_SIZE = CONFIG::TASKS::STACK_SENSOR_LOGGING;
    constexpr uint8_t TASK_PRIORITY = CONFIG::TASKS::PRIO_NORMAL;
    constexpr BaseType_t TASK_CORE = tskNO_AFFINITY;
    // Sensor read cadence is separate from downstream logging cadence on purpose.
    // SCD4x produces a new sample roughly every 5 s, while UI/history can remain
    // decimated to 10 s without starving alarms/live telemetry of fresh input.
    constexpr uint32_t READ_INTERVAL_MS = 5000;
    constexpr uint32_t LOG_INTERVAL_MS = 10000;    // 10 sec (mini-charts / decimated logging)
    // Flash persistence stays coarse on purpose, but the smoothing window should
    // be expressed in wall-clock time instead of a hard-coded sample count so it
    // remains stable if the sensor cadence changes again later. That keeps
    // history-on-flash representative of a fixed recent time slice instead of
    // drifting whenever READ_INTERVAL_MS changes.
    constexpr uint32_t FLASH_AGGREGATION_WINDOW_MS = 5 * 60 * 1000;
    constexpr size_t FLASH_AGGREGATION_WINDOW_SAMPLES =
        (FLASH_AGGREGATION_WINDOW_MS + READ_INTERVAL_MS - 1) / READ_INTERVAL_MS;
    constexpr uint32_t TASK_LOOP_SLEEP_MS = 500;
}

// ============================================================================
// Configuration Keys (from App.h)
// ============================================================================
#include "json/ConfigKeys.h"

// ============================================================================
// Factory Defaults (from App.h)
// ============================================================================
#ifndef FACTORY_BLE_ENABLED
#define FACTORY_BLE_ENABLED false
#endif

#ifndef FACTORY_NOTIFICATION_MODE
#define FACTORY_NOTIFICATION_MODE 0
#endif

#ifndef FACTORY_TELEGRAM_BOT_TOKEN
#define FACTORY_TELEGRAM_BOT_TOKEN ""
#endif

#ifndef FACTORY_TELEGRAM_CHAT_ID
#define FACTORY_TELEGRAM_CHAT_ID ""
#endif

#ifndef FACTORY_TELEGRAM_COMMANDS_ENABLED
#define FACTORY_TELEGRAM_COMMANDS_ENABLED false
#endif

#ifndef FACTORY_WEBHOOK_URL
#define FACTORY_WEBHOOK_URL ""
#endif

#ifndef FACTORY_WIFI_SENSING_ENABLED
#define FACTORY_WIFI_SENSING_ENABLED false
#endif

#ifndef FACTORY_WIFI_SENSING_SAMPLE_INTERVAL_MS
#define FACTORY_WIFI_SENSING_SAMPLE_INTERVAL_MS 200
#endif

#ifndef FACTORY_WIFI_SENSING_VARIANCE_THRESHOLD
#define FACTORY_WIFI_SENSING_VARIANCE_THRESHOLD 4.0f
#endif

#ifndef FACTORY_POWER_SLEEP_ENABLED
#define FACTORY_POWER_SLEEP_ENABLED false
#endif

#ifndef FACTORY_POWER_INACTIVITY_TIMEOUT_MS
#define FACTORY_POWER_INACTIVITY_TIMEOUT_MS 300000
#endif

#ifndef FACTORY_POWER_GRACE_AFTER_BOOT_MS
#define FACTORY_POWER_GRACE_AFTER_BOOT_MS 60000
#endif

#ifndef FACTORY_LOG_LEVEL
#define FACTORY_LOG_LEVEL 3
#endif




#ifndef TELEGRAM_TLS_VERIFY
#define TELEGRAM_TLS_VERIFY 0
#endif

// --- Merged from src/config/AppConfig.h ---
namespace APP {
    constexpr const char* VERSION = "1.0.0";
    constexpr const char* NAME = "MatrixHub";
    constexpr const char* DEVICE = "WAVESHARE-ESP32-S3-MATRIX";
    
    constexpr uint32_t MAIN_LOOP_DELAY_MS = 10;
}
#ifndef DIAG_ENABLE_BOOT_TIMING
#define DIAG_ENABLE_BOOT_TIMING 1
#endif

#ifndef DIAG_ENABLE_RUNTIME_TIMING
#define DIAG_ENABLE_RUNTIME_TIMING 0
#endif

#ifndef DIAG_ENABLE_RUNTIME_TASK_SNAPSHOT
#define DIAG_ENABLE_RUNTIME_TASK_SNAPSHOT 0
#endif

namespace DIAGNOSTICS {
    constexpr bool ENABLE_BOOT_TIMING = DIAG_ENABLE_BOOT_TIMING != 0;
    constexpr bool ENABLE_RUNTIME_TIMING = DIAG_ENABLE_RUNTIME_TIMING != 0;
    constexpr bool ENABLE_RUNTIME_TASK_SNAPSHOT = DIAG_ENABLE_RUNTIME_TASK_SNAPSHOT != 0;
}
namespace BOOT {
    namespace FILESYSTEM {
        // StorageInitializer owns LittleFS mount/recovery decisions. On cold
        // boot we prefer recovering to factory defaults over entering a reboot
        // loop when the on-flash FS was invalidated by a partition/layout
        // change or real corruption.
        constexpr bool FORMAT_ON_FAIL = true;
    }

    namespace FRAMEWORK {
        // Framework must reuse the filesystem mounted in Phase 1 instead of remounting it.
        constexpr bool MOUNT_FILESYSTEM = false;
    }

    namespace WATCHDOG {
        constexpr bool PANIC_ON_TIMEOUT = true;
    }

    namespace CSI {
        constexpr bool START_ENABLED = false;
    }
}
namespace FACTORY {
    constexpr uint32_t PROGRESS_LOG_INTERVAL_MS = 2000;
    constexpr uint32_t PRE_RESTART_DELAY_MS = 500;
    constexpr uint32_t SHORT_PRESS_DEBOUNCE_MS = 5;
    constexpr uint32_t SHORT_PRESS_MAX_MS = 1000;
    // Menu enter/exit fires immediately when the hold crosses this threshold,
    // so users don't need to release the button to open the menu.
    constexpr uint32_t MEDIUM_PRESS_MS = 2000;
    // Visual heads-up that a factory reset is about to be armed.
    constexpr uint32_t RESET_WARNING_MS = 7000;
    // The hold time at which factory reset becomes *armed* (not yet executed).
    // After release, the device waits for a confirmation double-click before
    // wiping config. This protects against a stuck button accidentally
    // bricking the device.
    constexpr uint32_t LONG_PRESS_MS = 10000;
    // After release from a 10s+ hold, the user has this long to press the
    // button twice to confirm the factory reset.
    constexpr uint32_t RESET_CONFIRM_WINDOW_MS = 5000;
    constexpr uint32_t HOLD_LOG_INTERVAL_MS = 2000;
}
namespace MEMORY {
    // 0 = Force PSRAM (Nuclear Option) - Maximizes SRAM for specific heavy loads
    // 4096 = Mixed Mode - Safer for DMA drivers (I2S/Matrix)
    constexpr size_t PSRAM_ALLOC_THRESHOLD = 16;
    
    // Force MbedTLS (HTTPS/SSL) to use PSRAM? (Saves ~30KB DRAM)
    constexpr bool USE_PSRAM_FOR_MBEDTLS = true;
}
namespace WORKER {
    constexpr uint32_t INIT_DELAY_MS = 3000;
}
namespace WATCHDOG {
    constexpr uint32_t TIMEOUT_SEC = 30;
}
namespace SHUTDOWN {
    constexpr uint32_t REGISTRY_CALLBACK_SETTLE_DELAY_MS = 50;
    constexpr uint32_t HTTP_CLIENT_CLOSE_GRACE_MS = 50;
    // prepareForSleep() is the final chance to refresh the retained config
    // snapshot before deep sleep. Give that path a slightly longer lock budget
    // than regular control-plane reads so we prefer a fresh warm-boot snapshot
    // over a fallback cold reload when shutdown is already in progress.
    constexpr uint32_t RTC_BACKUP_LOCK_TIMEOUT_MS = 500;
    constexpr uint32_t SERIAL_FLUSH_DELAY_MS = 50;
    constexpr uint32_t SERIAL_END_DELAY_MS = 10;
    constexpr uint32_t HARDWARE_SETTLE_DELAY_MS = 100;
}
