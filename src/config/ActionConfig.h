#pragma once

#include <Arduino.h>
#include <cstdint>
#include <algorithm>

namespace CONFIG {
namespace IMU {
    constexpr bool ENABLE_I2C_RECOVERY = true;
    constexpr uint32_t I2C_CLOCK_HZ = 400000;
    constexpr uint32_t I2C_TIMEOUT_MS = 10;
    constexpr uint32_t ACCEL_READ_LOCK_TIMEOUT_MS = 5;
    constexpr uint32_t BURST_READ_LOCK_TIMEOUT_MS = 10;
    constexpr uint32_t MANAGER_TRANSITION_TIMEOUT_MS = 250;
    constexpr uint8_t BURST_START_REGISTER = 0x35; // QMI8658_REG_AX_L
    constexpr size_t BURST_READ_BYTES = 12;
    constexpr int LOCK_TIMEOUT_TX_STATUS = -2;
}

namespace AIR_MOUSE {
    constexpr float DEFAULT_SENSITIVITY_X = 200.0f;
    constexpr float DEFAULT_SENSITIVITY_Y = 200.0f;
    constexpr float DEFAULT_DEADZONE = 2.0f;
    constexpr bool DEFAULT_MOVEMENT_ENABLED = false;
    constexpr bool DEFAULT_CLICK_ENABLED = false;
    constexpr bool DEFAULT_ACCELERATION_ENABLED = true;
    constexpr float DEFAULT_ACCELERATION_FACTOR = 1.0f;
    constexpr float DEFAULT_TAP_THRESHOLD_G = 0.4f;
    constexpr uint16_t DEFAULT_CLICK_DEBOUNCE_MS = 200;
    constexpr uint16_t DEFAULT_DOUBLE_CLICK_WINDOW_MS = 500;

    namespace FILTER {
        constexpr float DEFAULT_MIN_CUTOFF = 1.5f;
        constexpr float DEFAULT_BETA = 0.007f;
        constexpr float DEFAULT_D_CUTOFF = 1.0f;
    }

    namespace PROCESSOR {
        constexpr float GRAVITY_BASELINE_G = 1.0f;
        constexpr float ROLL_GATING_G = 0.2f; // Disable roll compensation when |accel|-1g exceeds this
        constexpr float GYRO_RANGE_DPS = 1024.0f;
        constexpr float GYRO_SAT_FRACTION = 0.95f;
        constexpr float SENSITIVITY_NORMALIZATION = 50.0f;
        // Global cursor gain applied after sensitivity and dt scaling.
        constexpr float OUTPUT_GAIN = 30.0f;
        constexpr float ACCELERATION_THRESHOLD = 8.0f;
        constexpr float ACCELERATION_MIN_SPEED = 0.1f;
        constexpr float ACCELERATION_NEUTRAL_MULTIPLIER = 1.0f;
        constexpr float ACCELERATION_MIN_MULTIPLIER = 0.2f;
        constexpr float ACCELERATION_MAX_MULTIPLIER = 12.0f;
    }

    namespace HID {
        constexpr uint32_t LOCK_TIMEOUT_MS = 50;
        constexpr int REPORT_DELTA_MIN = -127;
        constexpr int REPORT_DELTA_MAX = 127;
        constexpr int MAX_BACKLOG = 4096; // Max accumulated "tail" per axis (safety cap)
        constexpr uint8_t MAX_REPORTS_PER_TICK = 4; // Allow burst to avoid perceived slowdown
    }

    namespace LOCKS {
        constexpr uint32_t CONFIG_APPLY_TIMEOUT_MS = 100;
        constexpr uint32_t CONFIG_UPDATE_TIMEOUT_MS = 50;
        constexpr uint32_t BUTTON_CONFIG_TIMEOUT_MS = 20;
    }

    namespace LOOPS {
        constexpr uint32_t DISABLED_SLEEP_MS = 100;
        constexpr uint32_t PAUSED_SLEEP_MS = 10;
    }

    namespace JIGGLER {
        constexpr uint32_t DEFAULT_INTERVAL_S = 60;
        constexpr uint16_t DEFAULT_MOVE_DISTANCE = 1;
        constexpr bool DEFAULT_RANDOM_INTERVAL = false;
        constexpr uint32_t PAUSE_AFTER_ACTIVITY_MS = 5000;
        constexpr uint32_t MIN_INTERVAL_S = 1;
        constexpr uint16_t MIN_MOVE_DISTANCE = 1;
        constexpr uint32_t RANDOM_INTERVAL_SPREAD_DIVISOR = 2;
        constexpr int32_t MAX_MOUSE_STEP = 10;
        constexpr float HUMAN_BEZIER_T_STEP = 0.05f;
        constexpr uint8_t KEYBOARD_F13 = 0xF0;
    }
}

namespace KEYBOARD {
    constexpr uint32_t MUTEX_TIMEOUT_MS = 250;
    constexpr uint32_t MACOS_COMBO_HOLD_DELAY_MS = 120;
}

namespace MACRO {
    constexpr bool DEFAULT_ENABLED = false;
    constexpr uint32_t DEFAULT_BOOT_DELAY_MS = 5000;
}
}

namespace LIMITS {
    namespace AIR_MOUSE {
        constexpr float MIN_SENSITIVITY = 50.0f;
        constexpr float MAX_SENSITIVITY = 1000.0f;  // Increased for wider range
        constexpr float MIN_DEADZONE = 1.0f;
        constexpr float MAX_DEADZONE = 10.0f;
        constexpr float MIN_TAP_THRESHOLD = 0.02f;
        constexpr float MAX_TAP_THRESHOLD = 4.0f;
        constexpr uint16_t MIN_DEBOUNCE_MS = 50;
        constexpr uint16_t MAX_DEBOUNCE_MS = 600;
        constexpr uint16_t MIN_DOUBLE_CLICK_MS = 200;
        constexpr uint16_t MAX_DOUBLE_CLICK_MS = 1000;
        // Acceleration (non-linear speed curve)
        constexpr float MIN_ACCELERATION = 1.0f;   // 1.0 = linear (no acceleration)
        constexpr float MAX_ACCELERATION = 10.0f;   // higher = more acceleration
        // 1-Euro filter limits
        constexpr float MIN_EURO_CUTOFF = 0.1f;
        constexpr float MAX_EURO_CUTOFF = 10.0f;
        constexpr float MIN_EURO_BETA = 0.0f;
        constexpr float MAX_EURO_BETA = 1.0f;
        constexpr float MIN_EURO_D_CUTOFF = 0.1f;
        constexpr float MAX_EURO_D_CUTOFF = 10.0f;
        // Jiggler input limits (JSON validation) — safety bounds to avoid pathological intervals/moves.
        constexpr uint32_t MIN_JIGGLER_INTERVAL_S = 1;
        constexpr uint32_t MAX_JIGGLER_INTERVAL_S = 3600;
        constexpr uint16_t MIN_JIGGLER_MOVE_DISTANCE = 1;
        constexpr uint16_t MAX_JIGGLER_MOVE_DISTANCE = 5000;
        // Constants
        constexpr float GRAVITY_ACCEL = 9.81f;
        constexpr uint32_t CALIBRATION_COOLDOWN_MS = 1000;
    }
}
