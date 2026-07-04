/**
 * @file Network.h
 * @brief Network Configuration: WiFi, BLE, API Settings
 */

#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstddef>
#include <esp_bt.h>

// ============================================================================
// Network & API (from Network.h)
// ============================================================================
namespace NET {
    constexpr uint32_t CONNECT_TIMEOUT_MS = 10000;
    
    // HTTP Server Configuration
    namespace HTTP {
        #if defined(tskIDLE_PRIORITY)
        constexpr uint16_t kServerTaskPriorityBase = tskIDLE_PRIORITY;
        #else
        // Native/unit-test builds do not always provide the FreeRTOS idle
        // priority macro. Preserve the production offset while keeping host
        // config headers self-contained.
        constexpr uint16_t kServerTaskPriorityBase = 0;
        #endif
        // Keep this at the maximum profile we support here.
        // Do not lower it during "memory tuning": the bundled UI can legitimately
        // stack assets, REST bursts, WebSocket traffic, and live-tail requests at
        // the same time, and reducing this value causes avoidable timeouts.
        // If concurrency pressure returns, debug the callers/traffic mix first
        // instead of shrinking socket capacity again.
        constexpr uint16_t MAX_OPEN_SOCKETS = 13;
        // Allow a short burst of pending TCP/TLS connects to queue up while the
        // single-threaded httpd task is busy finishing another request.
        constexpr uint16_t BACKLOG_CONNECTIONS = 8;
        constexpr size_t MAX_REQUEST_HEADER_BYTES = 2048;
        constexpr bool LRU_PURGE_ENABLE = true;
        constexpr uint16_t SESSION_TIMEOUT_SEC = 5;  // recv/send timeout for HTTP server
        // Keep the HTTPS task above notification/network workers so local admin
        // traffic can accept, handshake, and clean up sockets during TLS load
        // spikes without waiting behind outbound Telegram work.
        // Leave the production relationship tied to the FreeRTOS idle priority;
        // host/native builds only fall back to the same relative offset when the
        // macro is unavailable.
        constexpr uint16_t SERVER_TASK_PRIORITY = kServerTaskPriorityBase + 7;
        constexpr uint32_t SERVER_STACK_SIZE_BYTES = 10240; // httpd_config_t expects bytes
        // Plain HTTP is only a tiny redirect shim to HTTPS. Keep its resource
        // profile intentionally small so enabling friendly browser entry does
        // not steal DRAM from the real TLS/UI server.
        constexpr uint16_t REDIRECT_MAX_OPEN_SOCKETS = 2;
        constexpr uint16_t REDIRECT_MAX_URI_HANDLERS = 1;
        constexpr uint16_t REDIRECT_BACKLOG_CONNECTIONS = 2;
        constexpr uint16_t REDIRECT_SESSION_TIMEOUT_SEC = 3;
        constexpr uint32_t REDIRECT_SERVER_STACK_SIZE_BYTES = 4096;
        constexpr uint16_t REDIRECT_CTRL_PORT = 20420;
        // Keep below the ESP-IDF 10 s default, but leave enough room for the
        // ESP32-S3 when thermal throttling, PSRAM TLS, or Telegram polling make
        // local WSS handshakes slower than the happy path.
        constexpr uint32_t TLS_HANDSHAKE_TIMEOUT_MS = 8000;
    }

    // JWT Token Configuration
    namespace AUTH {
        constexpr size_t   SIGN_IN_MAX_PAYLOAD_BYTES   = 1024;           // Max body on /rest/signIn
        // signIn performs PBKDF2 verification which is too heavy to run on the
        // single-threaded httpd task under HTTPS load.
        constexpr uint8_t  SIGN_IN_ASYNC_QUEUE_DEPTH   = 1;
        constexpr uint32_t SIGN_IN_ASYNC_STACK_BYTES   = 4096;
        constexpr uint32_t SIGN_IN_ASYNC_TASK_PRIORITY = 2;
        // Accepted risk for the current project phase: keep PBKDF2 iterations
        // deliberately low so on-device sign-in stays responsive and does not
        // overload the single-device admin workflow under HTTPS. Leave this in
        // place for now unless we are ready to ship a coordinated password
        // hardening pass, because increasing it later should come with a
        // rehash/migration plan for already stored credentials.
        constexpr uint32_t PASSWORD_HASH_ITERATIONS    = 20;
        constexpr size_t   PASSWORD_SALT_BYTES         = 16;             // 128-bit salt per password
        constexpr size_t   PASSWORD_DERIVED_KEY_BYTES  = 32;             // 256-bit PBKDF2 output
        constexpr const char* PASSWORD_HASH_SCHEME     = "pbkdf2_sha256";
    }
}

namespace API {
    constexpr uint32_t FS_MUTEX_TIMEOUT_MS = 500;
    constexpr uint32_t FILE_DOWNLOAD_FS_MUTEX_TIMEOUT_MS = 2000;
    constexpr uint32_t WS_CLIENT_HANDSHAKE_GRACE_MS = 250;
    constexpr uint32_t SENSOR_DATA_WAIT_MS = 2000;
    constexpr uint32_t SENSOR_MAX_AGE_MS = 30000;
    constexpr uint16_t DEFAULT_HTTP_PORT = 80;
    
    // API Endpoints
    constexpr const char* PATH_NOTIFICATION_SETTINGS = "/api/notifications/settings";
}

// ============================================================================
// BLE Configuration (from Network.h)
// ============================================================================
namespace BLE {

    // Feature Configuration
    // MatrixHub should keep scanner discovery alive in AP/AP+STA mode when the
    // platform can handle it. Set false only if a target-specific coexistence
    // regression forces conservative scanner shutdown on AP start.
    constexpr bool kAllowScannerInApMode = true;

    // NimBLE Stack
    constexpr const char* kDeviceName = "MatrixHub";
    // Scanner-only runtime still initializes NimBLE with a stable device name.
    
    // Power
    constexpr int kTransmitPowerLevel = ESP_PWR_LVL_P9; // +9dBm
    constexpr uint16_t kMaxMtu = 512;

    // Publish scanner readings to runtime diagnostics/Telegram without
    // rewriting persistent config on every advertising packet.
    constexpr uint32_t kRuntimeReadingsFlushIntervalMs = 500;

}

// --- Merged from src/config/NetworkConfig.h ---
namespace HEARTBEAT {
    constexpr uint32_t DEFAULT_INTERVAL_MS = 5 * 60 * 1000;
    constexpr uint32_t STARTUP_DELAY_MS = 60 * 1000;
    constexpr uint32_t SLOT_STAGGER_MS = 2000;
    constexpr uint32_t WAIT_SLICE_MS = 10000;
    constexpr uint32_t WAIT_MIN_MS = 100;
    constexpr uint32_t IDLE_CLIENT_RELEASE_MS = 30000;
    constexpr uint32_t HTTP_TIMEOUT_MS = 10000;
    constexpr uint8_t  HTTP_RETRIES    = 2;
}
namespace SYS_HEALTH {
    constexpr uint32_t HEAP_SAMPLE_STARTUP_DELAY_MS = 180000;
    constexpr uint32_t WIFI_HEALTH_RSSI_REFRESH_MS = 5000;
    constexpr uint32_t WIFI_HEALTH_STATE_RECONCILE_MS = 5000;
}
namespace INTEGRATION {
    namespace UDP {
        constexpr uint32_t STARTUP_DELAY_MS = 30 * 1000;
        constexpr uint32_t DEFAULT_INTERVAL_MS = 60 * 1000;
    }
}
namespace API {
    namespace TESTS {
        constexpr uint32_t GLOBAL_COOLDOWN_MS = 2000;
    }
}

namespace CONFIG {
namespace FILEMGR {
    constexpr uint32_t kUploadTimeoutMs = 30000;
    constexpr size_t kInlineMaxBytes = 4096;
    constexpr size_t kChunkSizeBytes = 1024;
    constexpr uint32_t kDownloadYieldMs = 1;
    constexpr size_t kDirListReserve = 32;
    constexpr uint32_t kDirListYieldMs = 1;
}
namespace NOTIFICATIONS {
    namespace WORKER {
        constexpr uint32_t ACTIVE_DELAY_MS = 50;
        constexpr uint32_t IDLE_DELAY_MS = 200;
        constexpr uint32_t INTER_CHANNEL_YIELD_MS = 1;
    }

    namespace WEBHOOK {
        constexpr size_t QUEUE_SIZE = 5;
        constexpr size_t MAX_PAYLOAD = 1024;
        constexpr size_t RESPONSE_PREVIEW_LEN = 64;
        constexpr uint32_t MUTEX_TIMEOUT_MS = 6000;
        constexpr uint32_t HTTP_TIMEOUT_MS = 5000;
        constexpr uint32_t QUEUE_BREATHER_MS = 1000;
        // Minimal hardening: keep the current single-worker design, but retry the
        // same alert a few times before we finally drop it on transient failures.
        constexpr uint8_t MAX_RETRY_ATTEMPTS = 3;
        constexpr uint32_t BACKOFF_BASE_MS = 2000;
        constexpr uint32_t BACKOFF_MAX_MS = 30000;
        constexpr uint8_t BACKOFF_MAX_EXPONENT = 4;
    }

    namespace PUSHOVER {
        constexpr size_t QUEUE_SIZE = 10;
        constexpr size_t MAX_MESSAGE_LEN = 1024;
        constexpr uint32_t MUTEX_TIMEOUT_MS = 6000;
        // Keep this shorter than before so one slow channel does not monopolize
        // the unified notification worker for 10 seconds per attempt.
        constexpr uint32_t HTTP_TIMEOUT_MS = 5000;
        constexpr uint32_t QUEUE_BREATHER_MS = 500;
        // Same bounded retry policy as Webhook: preserve the current alert across
        // brief network/TLS hiccups without introducing a larger queueing system.
        constexpr uint8_t MAX_RETRY_ATTEMPTS = 3;
        constexpr uint32_t BACKOFF_BASE_MS = 2000;
        constexpr uint32_t BACKOFF_MAX_MS = 60000;
        constexpr uint8_t BACKOFF_MAX_EXPONENT = 5;
    }
}
}

namespace LIMITS {
    namespace HEARTBEAT {
        constexpr uint32_t MIN_INTERVAL_MS = 1000;
        constexpr uint32_t MAX_INTERVAL_MS = 86400000;
    }
    namespace WIFI_SENSING {
        constexpr uint32_t MIN_INTERVAL_MS = 500;   // 2 FPS max - sufficient for motion detection
        constexpr uint32_t MAX_INTERVAL_MS = 5000;  // 5s max (aligned with frontend)
        constexpr float MIN_VARIANCE = 0.1f;
        constexpr float MAX_VARIANCE = 100.0f;
    }
    namespace UDP_PUSHER {
        constexpr uint32_t MIN_INTERVAL_MS = 500;
        constexpr uint32_t MAX_INTERVAL_MS = 3600000;
        constexpr uint16_t MIN_PORT = 1;
        constexpr uint16_t MAX_PORT = 65535;
    }
    namespace USB_TERMINAL {
        constexpr size_t MAX_OUTPUT_BUFFER_SIZE = 2504;
        constexpr size_t MAX_TOTAL_STREAM_BYTES = 10000;
        constexpr size_t PARTIAL_FLUSH_THRESHOLD_BYTES = 2000; 
        constexpr size_t MAX_ANSI_SEQ_LEN = 20;
        constexpr size_t MAX_TARGET_ID_LEN = 24;
        constexpr size_t MAX_STATUS_MESSAGE_LEN = 128;
        constexpr size_t MARKDOWN_WRAPPER_RESERVE_BYTES = 128; 
    }
    namespace API {
        // AirMouse IMU WebSocket streaming
        constexpr float IMU_ACCEL_DELTA_THRESHOLD = 0.2f;   // Min accel magnitude change to send
        constexpr float IMU_GYRO_DELTA_THRESHOLD = 5.0f;    // Min gyro axis change (dps) to send
        constexpr uint32_t IMU_WS_QUEUE_SIZE = 32;           // WebSocket broadcaster queue depth
        constexpr uint32_t IMU_WS_QUEUE_STACK = 4096;        // WebSocket broadcaster task stack
        constexpr uint32_t IMU_WS_QUEUE_PRIORITY = 5;        // WebSocket broadcaster task priority
        constexpr size_t IMU_STATUS_BUF_SIZE = 128;           // snprintf buffer for IMU status JSON
        // /ws/system is the highest-risk backend WS path for long uptime because
        // it mixes frequent small status packets with occasional multi-kB
        // snapshots. We keep the common path on fixed-size slots to reduce
        // fragmentation pressure, but still allow heap fallback for larger
        // payloads so uncommon channels keep working during the transition.
        constexpr uint32_t SYSTEM_WS_QUEUE_SIZE = 64;
        constexpr uint32_t SYSTEM_WS_QUEUE_STACK = 4096;
        constexpr size_t SYSTEM_WS_PAYLOAD_SLOT_SIZE = 8192;
        // Separate scratch buffers for JSON serialization. This removes one more
        // variable-size allocation from the hot reconnect/snapshot path.
        constexpr size_t SYSTEM_WS_SNAPSHOT_POOL_DEPTH = 2;
        constexpr size_t SYSTEM_WS_SNAPSHOT_SLOT_SIZE = SYSTEM_WS_PAYLOAD_SLOT_SIZE + 1;

        // System health thresholds
        constexpr uint32_t HEALTH_HEAP_FREE_MIN = 20000;     // Below this → "low_memory" warning
        constexpr uint32_t HEALTH_HEAP_LARGEST_MIN = 8000;   // Below this → "fragmented" warning
        constexpr size_t WS_MESSAGE_MAX_SIZE = 256;           // Max WebSocket message payload

        // Charts streaming
        constexpr size_t CHART_CHUNK_RECORDS = 512;           // Records per streaming chunk
        // Match the dashboard sparkline window so reconnect snapshots do not
        // ship extra history that the current frontend immediately trims away.
        constexpr size_t CHART_WS_HISTORY_POINTS = 48;        // Records to include in WS telemetry snapshot

        // JSON document capacities for API request parsing (PSRAM)
        namespace JSON_DOC {
            constexpr size_t CHANNEL_SUBSCRIPTIONS = 256;
            constexpr size_t SHELLY_RELAY_CONTROL = 512;
            constexpr size_t KEYBOARD = 1024;
            constexpr size_t POWER_CONFIG = 1024;
            constexpr size_t COMPENSATION_CONFIG = 1024;
            constexpr size_t USB_TERMINAL_CONFIG = 1024;
            constexpr size_t NOTIFICATIONS_PUSHOVER_TEST = 1024;
            constexpr size_t AIR_MOUSE_CONFIG = 2048;
            constexpr size_t MATRIX_CONFIG = 2048;
            constexpr size_t HEARTBEAT_CONFIG = 2048;
            constexpr size_t WIFI_SENSING_CONFIG = 2048;
            constexpr size_t MACROS_DEFAULT = 2048;
            constexpr size_t SHELLY_DEVICE_UPSERT = 4096;
            constexpr size_t NOTIFICATIONS_TELEGRAM_TEST = 4096;
            constexpr size_t NOTIFICATIONS_WEBHOOK_TEST = 4096;
            constexpr size_t LOGGING_CONFIG = 4096;
            constexpr size_t UDP_CONFIG = 4096;
            constexpr size_t ALARMS_RULES = 8192;
            constexpr size_t GPIO_CONFIG = 4096;
            constexpr size_t MACROS_UPLOAD = 16384;
        }
    }
}
