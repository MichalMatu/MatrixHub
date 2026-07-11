#pragma once

#include <Arduino.h>
#include <cstdint>
#include <algorithm>

#include <freertos/FreeRTOS.h>

namespace TASK_MONITOR {
    constexpr uint32_t STACK_LOG_INTERVAL_MS      = 73000;
    
    // Reporting intervals for profiling macros LOG_PROFILE_END_PERIODIC / SMART
    // Prime numbers are used to avoid resonance and simultaneous UART/I/O load
    constexpr uint32_t INTERVAL_TELEGRAM_FETCH_MS = 31000;
    constexpr uint32_t INTERVAL_TELEGRAM_DISP_MS  = 37000;
    constexpr uint32_t INTERVAL_AIRMOUSE_MS       = 41000;
    constexpr uint32_t INTERVAL_WIFI_RSSI_MS      = 43000;
    constexpr uint32_t INTERVAL_WIFI_CALLBACK_MS  = 47000;
    constexpr uint32_t INTERVAL_SHELLY_POLL_MS    = 53000;
    constexpr uint32_t INTERVAL_SHELLY_RELAY_MS   = 59000;
    constexpr uint32_t INTERVAL_PUSHOVER_MS       = 61000;
    constexpr uint32_t INTERVAL_WEBHOOK_MS        = 67000;
    constexpr uint32_t INTERVAL_TLS_HANDSHAKE_MS  = 71000;
    constexpr uint32_t INTERVAL_UDP_PUSH_MS       = 79000;
    constexpr uint32_t INTERVAL_FLASH_WRITE_MS    = 83000;
    constexpr uint32_t INTERVAL_I2C_READ_MS       = 89000;
    constexpr uint32_t INTERVAL_MAIN_LOOP_MS      = 97000;

    // Throttling intervals for non-critical warnings (ms)
    constexpr uint32_t BLE_WARNING_THROTTLE_MS    = 61000;
    constexpr uint32_t SLOW_LOG_THROTTLE_MS       = 5000;

    // Execution time thresholds (us) above which logging is always triggered
    constexpr uint32_t THRESHOLD_AIRMOUSE_US      = 10000;
    constexpr uint32_t THRESHOLD_SHELLY_POLL_US   = 500000;
    constexpr uint32_t THRESHOLD_SHELLY_RELAY_US  = 200000;
    constexpr uint32_t THRESHOLD_TELEGRAM_US      = 5000;
    constexpr uint32_t THRESHOLD_TELEGRAM_POST_US = 1000000;
    constexpr uint32_t THRESHOLD_FLASH_WRITE_US   = 100000;
    constexpr uint32_t THRESHOLD_I2C_READ_US      = 100000;
    constexpr uint32_t THRESHOLD_UDP_PUSH_US      = 100000;
    constexpr uint32_t THRESHOLD_TLS_HANDSHAKE_US = 5000000;
    constexpr uint32_t THRESHOLD_NOTIF_POST_US    = 2000000;
    constexpr uint32_t THRESHOLD_AIRMOUSE_MS      = (THRESHOLD_AIRMOUSE_US + 999u) / 1000u;
    constexpr uint32_t THRESHOLD_BUTTON_UPDATE_MS = 25;
    constexpr uint32_t THRESHOLD_IMU_READALL_MS   = 20;
    constexpr uint32_t THRESHOLD_MATRIX_TASK_MS   = 20;
    constexpr uint32_t THRESHOLD_RTC_CONFIG_READ_LOCK_MS = 20;
    constexpr uint32_t THRESHOLD_MAIN_LOOP_US     = 100000;
    constexpr uint32_t THRESHOLD_MAIN_LOOP_MS     = (THRESHOLD_MAIN_LOOP_US + 999u) / 1000u;
    constexpr uint32_t THRESHOLD_SYSTEM_HEALTH_UPDATE_MS = 50;
    constexpr uint32_t THRESHOLD_WIFI_HEALTH_UPDATE_MS = 50;
    constexpr uint32_t THRESHOLD_HEALTH_MAINTENANCE_UPDATE_MS = 50;
    constexpr uint32_t THRESHOLD_USB_TERMINAL_LOOP_MS = 50;
    constexpr uint32_t THRESHOLD_SOFT_WDT_LOOP_MS = 15000;
}
namespace TIMEOUT {
    // Mutex
    constexpr uint32_t MUTEX_STANDARD_MS = 100;
    constexpr TickType_t MUTEX_STANDARD_TICKS = pdMS_TO_TICKS(MUTEX_STANDARD_MS);
    constexpr uint32_t MUTEX_NETWORK_MS = 3000;
    constexpr TickType_t MUTEX_NETWORK_TICKS = pdMS_TO_TICKS(MUTEX_NETWORK_MS);
    constexpr uint32_t MUTEX_POLL_MS = 1000;
    constexpr TickType_t MUTEX_POLL_TICKS = pdMS_TO_TICKS(MUTEX_POLL_MS);
    constexpr uint32_t MUTEX_FS_MS = 100;
    constexpr TickType_t MUTEX_FS_TICKS = pdMS_TO_TICKS(MUTEX_FS_MS);
    constexpr uint32_t MUTEX_SHORT_MS = 10;
    constexpr TickType_t MUTEX_SHORT_TICKS = pdMS_TO_TICKS(MUTEX_SHORT_MS);

    // Queue
    constexpr uint32_t QUEUE_STANDARD_MS = 100;
    constexpr TickType_t QUEUE_STANDARD_TICKS = pdMS_TO_TICKS(QUEUE_STANDARD_MS);
    constexpr uint32_t QUEUE_LONG_MS = 5000;
    constexpr TickType_t QUEUE_LONG_TICKS = pdMS_TO_TICKS(QUEUE_LONG_MS);
    constexpr uint32_t QUEUE_NONBLOCK_MS = 0;
    constexpr TickType_t QUEUE_NONBLOCK_TICKS = pdMS_TO_TICKS(QUEUE_NONBLOCK_MS);

    // TLS/HTTP
    constexpr uint32_t TLS_CLIENT_LOCK_MS = 5000;
    constexpr uint32_t HTTP_CONNECT_MS = 3000;
    constexpr uint32_t HTTP_READWRITE_MS = 4000;
    constexpr uint32_t TELEGRAM_LONG_POLL_MAX_MS = 2000;
    constexpr uint32_t TELEGRAM_LONG_POLL_BUFFER_MS = 5000;

    // Task & System Shutdown
    constexpr uint32_t TASK_SHUTDOWN_MS = 5000;
    constexpr uint32_t REBOOT_COMMAND_DELAY_MS = 4000;
    constexpr TickType_t TASK_SHUTDOWN_TICKS = pdMS_TO_TICKS(TASK_SHUTDOWN_MS);
    constexpr uint32_t TASK_SHUTDOWN_POLL_MS = 50;
    constexpr TickType_t TASK_SHUTDOWN_POLL_TICKS = pdMS_TO_TICKS(TASK_SHUTDOWN_POLL_MS);
    // NotificationWorker shutdown (network tasks may block longer)
    constexpr uint32_t NOTIF_WORKER_STOP_MS = 8000;
    constexpr uint32_t NOTIF_WORKER_STOP_POLL_MS = 100;
    constexpr uint32_t HEARTBEAT_STOP_MS = 25000;
    constexpr uint32_t HEARTBEAT_STOP_POLL_MS = 100;

    // Heap Monitoring
    constexpr int32_t HEAP_DELTA_LOG_THRESHOLD = 100;
    constexpr uint32_t HEAP_MIN_SAFE = 10240;

    // Delays
    constexpr uint32_t DELAY_INIT_STEP_MS = 100;
    constexpr TickType_t DELAY_INIT_STEP_TICKS = pdMS_TO_TICKS(DELAY_INIT_STEP_MS);
    constexpr uint32_t DELAY_RATE_LIMIT_MS = 100;
    constexpr TickType_t DELAY_RATE_LIMIT_TICKS = pdMS_TO_TICKS(DELAY_RATE_LIMIT_MS);

    // USB Terminal
    namespace USB_TERMINAL {
        constexpr bool DEFAULT_ENABLED = false;
        constexpr uint32_t DEFAULT_IDLE_TIMEOUT_MS = 2000;
        constexpr uint32_t MIN_IDLE_TIMEOUT_MS = 500;
        constexpr uint32_t MAX_IDLE_TIMEOUT_MS = 30000;
        constexpr uint32_t PERIODIC_FLUSH_INTERVAL_MS = 5000;
    }
}

namespace CONFIG {
namespace TASKS {
    struct StackBudget {
        uint32_t stackBytes;
        uint32_t minFreeBytes;
    };

    // Stack sizes (bytes). ESP-IDF Xtensa uses StackType_t = uint8_t,
    // so stack depth arguments/arrays are byte-based (no word conversion).
    constexpr uint32_t STACK_TINY       = 2048;
    constexpr uint32_t STACK_SMALL      = 3072;
    constexpr uint32_t STACK_MEDIUM     = 4096;
    constexpr uint32_t STACK_MEDIUM_PLUS= 5120;
    constexpr uint32_t STACK_LARGE      = 6144;
    constexpr uint32_t STACK_LARGE_PLUS = 7168;
    constexpr uint32_t STACK_HUGE       = 8192;
    constexpr uint32_t STACK_MASSIVE    = 10240;


    // Core Definitions
    constexpr BaseType_t CORE_PRO       = 0;
    constexpr BaseType_t CORE_APP       = 1;
    constexpr BaseType_t CORE_ANY       = tskNO_AFFINITY;

    // --- System Tasks ---
    constexpr uint32_t STACK_HEARTBEAT       = STACK_HUGE; // Dedicated TLS task, lazy-loaded only when enabled
    // Level 1: Idle / Keep-Alive (CORE_PRO)
    constexpr UBaseType_t PRIO_HEARTBEAT     = 1;
    constexpr BaseType_t CORE_HEARTBEAT      = CORE_PRO;

    // --- Sensor & Data Tasks ---
    constexpr uint32_t STACK_SENSOR_LOGGING  = STACK_MASSIVE; // HWM ~7.5KB -> add headroom
    // Level 3: Shedable background workload (CORE_PRO) — environmental logs can wait.
    constexpr UBaseType_t PRIO_SENSOR_LOGGING= 3;
    constexpr BaseType_t CORE_SENSOR_LOGGING = 0; // Core 0: isolate I2C ISR from RMT/WS2812 on Core 1

    constexpr uint32_t STACK_WIFI_SENSING    = STACK_LARGE; // Default/internal WiFi sensing stack size.
    // The RSSI runner uses a PSRAM-backed static stack, so give it more room
    // without spending internal DRAM.
    constexpr uint32_t STACK_WIFI_SENSING_RSSI = STACK_HUGE;
    // CSI processing keeps an internal stack because it drives networking-facing
    // code paths; keep it on the tighter internal budget.
    constexpr uint32_t STACK_WIFI_SENSING_CSI  = STACK_WIFI_SENSING;
    constexpr uint32_t STACK_WIFI_SENSING_RECONCILE = STACK_WIFI_SENSING;
    constexpr uint32_t STACK_CSI_FRONTEND_STOP = STACK_MEDIUM;
    constexpr uint32_t STACK_CSI_CAPTURE_STOP  = STACK_MEDIUM;
    // Level 3: Shedable background workload (CORE_PRO) — CSI is a non-critical gadget.
    constexpr UBaseType_t PRIO_WIFI_SENSING  = 3;
    constexpr BaseType_t CORE_WIFI_SENSING   = CORE_PRO;
    constexpr UBaseType_t PRIO_WIFI_SENSING_RECONCILE = 2;
    constexpr BaseType_t CORE_WIFI_SENSING_RECONCILE = CORE_PRO;
    constexpr UBaseType_t PRIO_CSI_FRONTEND_STOP = PRIO_WIFI_SENSING;
    constexpr BaseType_t CORE_CSI_FRONTEND_STOP = CORE_WIFI_SENSING;
    constexpr UBaseType_t PRIO_CSI_CAPTURE_STOP = PRIO_WIFI_SENSING;
    constexpr BaseType_t CORE_CSI_CAPTURE_STOP = CORE_WIFI_SENSING;

    // --- BLE Tasks ---
    constexpr uint32_t STACK_BLE_HOST        = 4096; // Synced with lib/NimBLE-Arduino/src/nimconfig.h
    constexpr uint32_t STACK_BLE_SERVICE     = 4096;
    // Level 1: Idle / Keep-Alive (CORE_PRO)
    constexpr UBaseType_t PRIO_BLE_SERVICE   = 1;
    constexpr BaseType_t CORE_BLE_SERVICE    = CORE_PRO;

    constexpr uint32_t STACK_BLE_DATA        = STACK_MEDIUM;      // Peak usage: 2212 bytes
    // Level 1: Idle / Keep-Alive (CORE_PRO)
    constexpr UBaseType_t PRIO_BLE_DATA      = 1;
    constexpr BaseType_t CORE_BLE_DATA       = CORE_PRO;

    // --- Network & Notification Tasks ---
    constexpr uint32_t STACK_TELEGRAM        = STACK_MASSIVE; // 10KB (was 8KB)
    constexpr uint32_t STACK_NOTIFICATION_WORKER = STACK_HUGE;
    constexpr uint32_t STACK_REBOOT          = STACK_HUGE; // Uses ShutdownSequence

    constexpr uint32_t STACK_WEBHOOK         = STACK_HUGE;
    // Manual notification diagnostics can exercise the same synchronous TLS
    // paths as the production senders, so size this detached worker after the
    // heaviest notification channel instead of repeating per-channel values.
    // If only one manual test endpoint starts crashing after sender changes,
    // inspect this ceiling before changing the scheduler code again.
    constexpr uint32_t STACK_NOTIFICATION_TEST = std::max(STACK_TELEGRAM, STACK_WEBHOOK);
    constexpr uint32_t STACK_WIFI_RECOVERY   = STACK_MEDIUM;
    // Level 6: Critical Network & Alerts (CORE_PRO)
    constexpr UBaseType_t PRIO_WIFI_RECOVERY = 6;
    constexpr BaseType_t CORE_WIFI_RECOVERY  = CORE_PRO;

    // Level 6: Critical Network & Alerts (CORE_PRO)
    constexpr UBaseType_t PRIO_NOTIFICATION  = 6;
    constexpr BaseType_t CORE_NOTIFICATION   = CORE_PRO;

    // Manual notification tests are useful diagnostics, but they should stay
    // below the real notification worker and other critical recovery paths.
    // If test tasks become starved or start delaying production recovery work,
    // debug these dedicated constants rather than reusing generic background
    // priorities inside NotificationTestJobScheduler.
    constexpr UBaseType_t PRIO_NOTIFICATION_TEST = 2;
    constexpr BaseType_t CORE_NOTIFICATION_TEST = CORE_NOTIFICATION;

    // Stack HWM budgets are warning floors, not resize targets. Keep the stack
    // sizes above unchanged until target-device logs show sustained headroom
    // under the specific feature load.
    constexpr uint32_t STACK_MIN_FREE_BACKGROUND = 1536;
    constexpr uint32_t STACK_MIN_FREE_TLS        = 2048;
    constexpr uint32_t STACK_MIN_FREE_SENSOR_FS  = 2048;
    constexpr StackBudget STACK_BUDGET_SENSOR_LOGGING{
        STACK_SENSOR_LOGGING,
        STACK_MIN_FREE_SENSOR_FS
    };
    constexpr StackBudget STACK_BUDGET_NOTIFICATION_WORKER{
        STACK_NOTIFICATION_WORKER,
        STACK_MIN_FREE_TLS
    };
    constexpr StackBudget STACK_BUDGET_HEARTBEAT{
        STACK_HEARTBEAT,
        STACK_MIN_FREE_TLS
    };
    constexpr StackBudget STACK_BUDGET_WIFI_SENSING_CSI{
        STACK_WIFI_SENSING_CSI,
        STACK_MIN_FREE_BACKGROUND
    };

    // WebSocket broadcast (HTTP/WS streaming)
    // Level 4: Stable I/O & streaming (CORE_PRO)
    constexpr UBaseType_t PRIO_WS_BROADCAST  = 4;
    constexpr BaseType_t CORE_WS_BROADCAST   = CORE_PRO;

    // Level 5: Reserved buffer (no tasks assigned by design).

    constexpr uint32_t STACK_UDP_PUSHER      = STACK_MEDIUM;
    // Level 2: Low-priority background (CORE_PRO)
    constexpr UBaseType_t PRIO_UDP_PUSHER    = 2; // Background priority for LAN UDP sends
    constexpr BaseType_t CORE_UDP_PUSHER     = CORE_PRO;

    constexpr uint32_t STACK_GPIO_TASK       = STACK_SMALL;
    constexpr UBaseType_t PRIO_GPIO_TASK     = 1;
    constexpr BaseType_t CORE_GPIO_TASK      = CORE_PRO;

    constexpr uint32_t STACK_SHELLY          = STACK_LARGE; // +2 KB vs 4 KB: recent coredump left ~80 B free in ShellyWorker
    // Level 4: Stable I/O & streaming (CORE_PRO)
    constexpr UBaseType_t PRIO_SHELLY        = 4;
    constexpr BaseType_t CORE_SHELLY         = CORE_PRO;

    // --- UI & Visual Tasks ---
    // MatrixTask now owns LED rendering plus Matrix data visualization inputs
    // (BLE/RSSI/CSI). CSI activation can run a deeper service path from this
    // task, so keep generous PSRAM-backed headroom instead of sizing it like a
    // tiny renderer.
    constexpr uint32_t STACK_MATRIX_TASK     = STACK_HUGE;
    // CORE_APP isolation: MatrixTask (prio 2) must preempt AirMouse/Macro/Thermal (prio 1)
    // to avoid LED jitter. Do not change these priorities without a full scheduler audit.
    // After moving httpd/heartbeat off CORE_APP, the remaining visible jitter
    // under LED effects is dominated by MatrixTask missing its frame budget.
    // A small bump above loopTask helps rendering cadence without bringing back
    // the old HTTP starvation that forced us to keep it at priority 1.
    constexpr UBaseType_t PRIO_MATRIX_TASK   = 2;
    constexpr BaseType_t CORE_MATRIX_TASK    = CORE_APP;



    // --- Thermal Monitoring Task ---
    constexpr uint32_t STACK_THERMAL_MONITOR = STACK_SMALL;
    constexpr UBaseType_t PRIO_THERMAL_MONITOR = 1;
    constexpr BaseType_t CORE_THERMAL_MONITOR  = CORE_APP;

    // --- IMU Sampler Task ---
    constexpr uint32_t STACK_IMU_SAMPLER      = STACK_MEDIUM_PLUS;
    // Keep the IMU sampler on CORE_PRO to avoid I2C vs RMT contention on CORE_APP,
    // but keep it below critical network tasks so WiFi/TLS remain responsive.
    // Level 4: Stable I/O & streaming (CORE_PRO).
    constexpr UBaseType_t PRIO_IMU_SAMPLER    = 4;
    constexpr BaseType_t CORE_IMU_SAMPLER     = CORE_PRO;

    // --- Air Mouse Task ---
    constexpr uint32_t STACK_AIR_MOUSE       = STACK_LARGE;
    // Keep AirMouse at loopTask priority on CORE_APP. After raising IMU I2C to
    // 400 kHz, a higher priority caused AirMouse bursts to dominate loopTask.
    constexpr UBaseType_t PRIO_AIR_MOUSE     = 1;
    constexpr BaseType_t CORE_AIR_MOUSE      = CORE_APP;

    // --- Macro Task ---
    constexpr uint32_t STACK_MACRO           = STACK_MEDIUM_PLUS; // Increased from STACK_MEDIUM to give +1024B (HWM safety)
    constexpr UBaseType_t PRIO_MACRO         = 1;
    constexpr BaseType_t CORE_MACRO          = CORE_APP;
    constexpr uint32_t STACK_MACRO_BOOT      = STACK_MEDIUM_PLUS;
    constexpr uint32_t MATRIX_ACTIVE_INTERVAL_MS = 31;       // ~32 FPS (Prime to avoid resonance)
    constexpr uint32_t MATRIX_IDLE_INTERVAL_MS   = 200;      // 5 FPS for standby mode (reduces CPU load)
    constexpr uint32_t AIR_MOUSE_INTERVAL_MS = 10;       // 100 Hz (stable cursor cadence)
    constexpr uint32_t AIR_MOUSE_CALIB_SAMPLES = 100;    // Calibration samples
    constexpr uint32_t AIR_MOUSE_STARTUP_DELAY_COLD_BOOT_MS = 500;
    constexpr uint32_t AIR_MOUSE_STARTUP_DELAY_WARM_BOOT_MS = 200;
    constexpr uint32_t AIR_MOUSE_CALIB_SAMPLE_DELAY_MS = 10;
    constexpr uint32_t AIR_MOUSE_JIGGLER_ONLY_INTERVAL_MS = 100;
    constexpr uint32_t AIR_MOUSE_IMU_ERROR_LOG_THROTTLE_MS = 2000;
    // AirMouse consumes cached data at ~100 Hz, so keep the IMU sampler aligned.
    constexpr uint32_t IMU_SAMPLE_INTERVAL_MS = 10;
    // Real logs still show occasional successful sampler reads taking ~200 ms.
    // Keep using the last good sample through short CORE_PRO / I2C hiccups
    // instead of dropping to "cached sample unavailable" immediately.
    constexpr uint32_t IMU_SAMPLE_STALE_MS    = 800;
    // --- Deprecated Helpers ---
    constexpr UBaseType_t PRIO_LOW           = 1; 
    constexpr UBaseType_t PRIO_BACKGROUND    = 2;
    constexpr UBaseType_t PRIO_NORMAL        = 3;
}
}
