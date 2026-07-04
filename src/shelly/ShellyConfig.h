#pragma once

#include <cstdint>
#include <cstddef>
#include "../config/System.h"

namespace SHELLY {

// ============================================================================
// Device Limits
// ============================================================================
constexpr size_t kMaxShellyName = 64;
constexpr size_t kMaxShellyIp = 16;
constexpr size_t kMaxShellyId = 32;
constexpr size_t kMaxDevices = 4;  // Maximum number of Shelly devices

// ============================================================================
// Filesystem
// ============================================================================
// kShellyConfigFile removed - now managed by ConfigManager


// ============================================================================
// HTTP Configuration
// ============================================================================
constexpr uint8_t kHttpMaxRetries = 3;
constexpr uint16_t kHttpRetryDelayMs = 200;
constexpr uint16_t kHttpTimeoutMs = 1500;
constexpr uint16_t kHttpPollTimeoutMs = 2000;

// ============================================================================
// Worker Task Configuration
// ============================================================================
constexpr unsigned long kPollIntervalMs = 10000;      // Poll every 10s
constexpr unsigned long kShutdownTimeoutMs = 12000;    // Max wait for graceful shutdown (12s to cover network timeouts)
constexpr uint16_t kCommandQueueSize = 10;            // Max queued commands
constexpr uint16_t kWorkerStackSize = CONFIG::TASKS::STACK_SHELLY;
constexpr uint8_t kWorkerPriority = CONFIG::TASKS::PRIO_SHELLY;                // FreeRTOS task priority

} // namespace SHELLY
