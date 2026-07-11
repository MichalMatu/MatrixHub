#pragma once

namespace API {

// Capture admission is intentionally stricter than desired-consumer ownership.
// A retained source queue from a failed startup must never be presented as a
// healthy real-device capture session.
constexpr bool isCsiCaptureStartReady(bool activationAccepted,
                                      bool websocketQueueReady,
                                      bool serviceRuntimeReady,
                                      bool serviceEnabled,
                                      bool captureConsumerActive,
                                      bool sourceQueueMetricsReady) {
    return activationAccepted && websocketQueueReady && serviceRuntimeReady &&
           serviceEnabled && captureConsumerActive && sourceQueueMetricsReady;
}

} // namespace API
