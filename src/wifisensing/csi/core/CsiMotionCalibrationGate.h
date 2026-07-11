#pragma once

#include <stdint.h>

#include "../algo/CsiMotionTypes.h"

namespace WIFISENSING {
namespace CSI {

// Fail-safe preconditions for an explicit baseline reset.  A manual
// calibration authorizes the detector to forget a retained active decision,
// so it is only safe while the acquisition pipeline is demonstrably live.
class CsiMotionCalibrationGate {
public:
    static bool canStart(bool configEnabled,
                         uint8_t bandCount,
                         bool runtimeEnabled,
                         bool storageReady,
                         bool gainForced,
                         bool runtimeFault,
                         const CsiMotionSnapshot& snapshot,
                         uint32_t nowMs,
                         uint32_t staleAfterMs) {
        return configEnabled &&
               bandCount > 0 &&
               runtimeEnabled &&
               storageReady &&
               gainForced &&
               !runtimeFault &&
               snapshot.state != CsiMotionState::Unavailable &&
               snapshot.hasFrame &&
               (nowMs - snapshot.lastFrameMs) <= staleAfterMs;
    }
};

} // namespace CSI
} // namespace WIFISENSING
