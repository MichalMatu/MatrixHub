#pragma once

namespace WIFISENSING {
namespace CSI {

// Queue and semaphore storage is shared with both the Wi-Fi callback and the
// processing task. Every owner must be proven inactive before it is released.
constexpr bool canReleaseCsiRuntimeResources(bool callbackDetached,
                                             bool callbacksDrained,
                                             bool processingTaskStopped) {
    return callbackDetached && callbacksDrained && processingTaskStopped;
}

} // namespace CSI
} // namespace WIFISENSING
