#pragma once

namespace WIFISENSING {
namespace CSI {

// Pure readiness predicate used by the capture admission path. A desired
// consumer bit or a retained queue alone is not evidence that real CSI frames
// can currently reach the processing task.
constexpr bool isCsiRuntimeReadyState(bool enabled,
                                      bool queueReady,
                                      bool processingTaskReady,
                                      bool processingStopRequested,
                                      bool pingReady,
                                      bool rxCallbackRegistered,
                                      bool rxCallbackEnabled) {
    return enabled && queueReady && processingTaskReady &&
           !processingStopRequested && pingReady && rxCallbackRegistered &&
           rxCallbackEnabled;
}

} // namespace CSI
} // namespace WIFISENSING
