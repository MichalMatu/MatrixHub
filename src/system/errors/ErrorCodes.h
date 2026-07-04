/**
 * @file ErrorCodes.h
 * @brief Centralized error code definitions for API responses
 * 
 * This file provides a single source of truth for all error codes used
 * in API responses. Using constants instead of magic strings provides:
 * - Compile-time checking (typos become compile errors)
 * - IDE autocomplete support
 * - Easy refactoring (change format in one place)
 * - Clear documentation of all possible errors
 * 
 * Error codes follow the pattern: "category/specific_error"
 * 
 * Usage example:
 * @code
 * #include "system/ErrorCodes.h"
 * 
 * root["error"] = ErrorCodes::Config::NOT_CONFIGURED;
 * root["error"] = ErrorCodes::Input::EMPTY_TEXT;
 * @endcode
 */

#pragma once

namespace ErrorCodes {

/**
 * Configuration-related errors
 * Used when service or feature is not properly configured
 */
namespace Config {
    constexpr const char* NOT_CONFIGURED = "config/not_configured";
    constexpr const char* INVALID_VALUE = "config/invalid_value";
}

/**
 * Authentication and authorization errors
 * Used for auth-related failures (login, JWT, permissions)
 */
namespace Auth {
    constexpr const char* UNAUTHORIZED = "auth/unauthorized";
    constexpr const char* FORBIDDEN = "auth/forbidden";
    constexpr const char* RATE_LIMITED = "auth/rate_limited";
    constexpr const char* LOGIN_RATE_LIMITED = "auth/login_rate_limited";
    constexpr const char* INVALID_CREDENTIALS = "auth/invalid_credentials";
    constexpr const char* MISSING_CREDENTIALS = "auth/missing_credentials";
}

/**
 * Input validation errors
 * Used when user-provided input fails validation
 */
namespace Input {
    constexpr const char* JSON_PARSE_ERROR = "input/json_parse_error";
    constexpr const char* PAYLOAD_TOO_LARGE = "input/payload_too_large";
    constexpr const char* EMPTY_TEXT = "input/empty_text";
    constexpr const char* TEXT_TOO_LONG = "input/text_too_long";
    constexpr const char* INVALID_RANGE = "input/invalid_range";
    constexpr const char* INVALID_FORMAT = "input/invalid_format";
    constexpr const char* MISSING_FIELD = "input/missing_field";
    constexpr const char* INVALID_TIME_FORMAT = "input/invalid_time_format";
    constexpr const char* YEAR_OUT_OF_RANGE = "input/year_out_of_range";
    constexpr const char* INVALID_JSON = "input/invalid_json";
}

/**
 * Filesystem / file manager errors
 * Used for file operations (upload, download, delete, list)
 */
namespace Fs {
    constexpr const char* INVALID_PATH = "fs/invalid_path";
    constexpr const char* PATH_FORBIDDEN = "fs/path_forbidden";
    constexpr const char* PATH_MISSING = "fs/path_missing";
    constexpr const char* FILE_NOT_FOUND = "fs/file_not_found";
    constexpr const char* OPEN_FAILED = "fs/open_failed";
    constexpr const char* DELETE_FAILED = "fs/delete_failed";
    constexpr const char* UPLOAD_FAILED = "fs/upload_failed";
    constexpr const char* ALREADY_EXISTS = "fs/already_exists";
    constexpr const char* STORAGE_FULL = "fs/storage_full";
    constexpr const char* CONFIG_WRITE_FORBIDDEN = "fs/config_write_forbidden";
}

/**
 * Log/history-specific errors
 * Used when log maintenance or admin actions would conflict with the active
 * binary logger runtime.
 */
namespace Logs {
    constexpr const char* ACTIVE_FILE = "logs/active_file";
}

/**
 * NTP / time-related errors
 */
namespace Ntp {
    constexpr const char* MANUAL_TIME_REQUIRES_NTP_DISABLED = "ntp/manual_time_requires_ntp_disabled";
    constexpr const char* MISSING_LOCAL_TIME = "ntp/missing_local_time";
}

/**
 * Service availability errors
 * Used when a required service is unavailable or not initialized
 */
namespace Service {
    constexpr const char* UNAVAILABLE = "service/unavailable";
    constexpr const char* TELEGRAM_SETTINGS_UNAVAILABLE = 
        "service/telegram_settings_unavailable";
}

/**
 * Busy/concurrency errors
 * Used when resource is temporarily unavailable due to concurrent access
 */
namespace Busy {
    constexpr const char* TELEGRAM_TEST_IN_PROGRESS = 
        "busy/telegram_test_in_progress";
    constexpr const char* FILESYSTEM_BUSY = "busy/filesystem";
    constexpr const char* RESOURCE_LOCKED = "busy/resource_locked";
}

/**
 * Shelly integration errors
 * Used by Shelly device CRUD/control endpoints and clients.
 */
namespace Shelly {
    constexpr const char* DEVICE_NOT_FOUND = "shelly/device_not_found";
    constexpr const char* DEVICE_DISABLED = "shelly/device_disabled";
    constexpr const char* CONTROL_NOT_QUEUED = "shelly/control_not_queued";
}

/**
 * Internal/system errors
 * Used for unexpected internal failures (allocation, task creation, etc.)
 */
namespace Internal {
    constexpr const char* ALLOC_FAILED = "internal/alloc_failed";
    constexpr const char* OUT_OF_MEMORY = "internal/out_of_memory";
    constexpr const char* TASK_CREATE_FAILED = "internal/task_create_failed";
    constexpr const char* TASK_TIMEOUT = "internal/task_timeout";
    constexpr const char* UNEXPECTED_ERROR = "internal/unexpected_error";
}

}  // namespace ErrorCodes
