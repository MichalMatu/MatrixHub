#pragma once

#include "HeartbeatWorker.h"

namespace SYSTEM {

/**
 * @brief Public facade for the standalone dead man's switch heartbeat service.
 *
 * The public entry points stay in this thin header while the worker internals
 * remain inside the heartbeat submodule.
 */
class Heartbeat {
public:
    static void begin() {
        HEARTBEAT_DETAIL::heartbeatWorker().begin();
    }

    static bool pingNow() {
        return HEARTBEAT_DETAIL::heartbeatWorker().pingNow();
    }

    static void checkState() {
        HEARTBEAT_DETAIL::heartbeatWorker().checkState();
    }

    static void stop() {
        HEARTBEAT_DETAIL::heartbeatWorker().stop();
    }
};

} // namespace SYSTEM
