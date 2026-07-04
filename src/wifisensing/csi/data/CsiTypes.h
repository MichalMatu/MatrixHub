#pragma once

#include <Arduino.h>
#include <esp_wifi_types.h>
#include <functional>

namespace WIFISENSING {
namespace CSI {

// Fixed-size handoff object between the Wi-Fi CSI RX callback and the worker task.
// Keeping this struct bounded and trivially copyable lets the callback memcpy raw
// driver data into the queue without any heap allocation.
// Max HT40 CSI data is ~384 bytes. We allow some headroom.
constexpr size_t MAX_CSI_DATA_LEN = 512;
constexpr size_t MAX_CSI_BATCH_PACKETS = 10;

struct CsiPacket {
    wifi_pkt_rx_ctrl_t rx_ctrl;
    uint8_t mac[6];
    int8_t buf[MAX_CSI_DATA_LEN];
    size_t len;
    float compensate_gain; // Filled by the processing task after dequeue, not in ISR.
    
    // Filled by the processing task after CsiBandMotionDetector evaluates the packet.
    // These fields are part of the stable /ws/csi record header.
    float motionScore;
    bool isMotionDetected;
};

// Callback function type for processing CSI packets (Batched)
using CsiCallback = std::function<void(const CsiPacket* data, size_t count)>;
// Callback for motion state change (true = motion start, false = motion stop)
using MotionCallback = std::function<void(bool)>;

} // namespace CSI
} // namespace WIFISENSING
