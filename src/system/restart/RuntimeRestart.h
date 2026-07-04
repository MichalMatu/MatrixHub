#pragma once

#include "../boot/BootTracker.h"

#include <cstdint>

namespace SYSTEM::RESTART {

/**
 * Intentionally short restart path for runtime fault recovery.
 *
 * Unlike planned restarts, this helper does not run ShutdownSequence. It is
 * used only when the process is already in a degraded state (for example a
 * lost main-loop watchdog heartbeat or critically low heap) and the safest
 * action is to persist the reboot reason and restart promptly.
 */
void emergencyRestart(SYSTEM::ShutdownReason reason,
                      const char* logTag,
                      const char* message,
                      uint32_t delayMs = 100);

}  // namespace SYSTEM::RESTART
