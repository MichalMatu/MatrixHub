#pragma once

#include <cstring>
#include <type_traits>
#include <utility>

#include "CsiCaptureFixtureCodec.h"
#include "../../src/wifisensing/csi/algo/CsiBandMotionDetector.h"

namespace WIFISENSING {
namespace CSI {
namespace NATIVE_CAPTURE {

struct CsiCaptureReplayResult {
    uint32_t framesReplayed = 0;
    uint32_t firstAcceptedSeq = 0;
    uint32_t lastAcceptedSeq = 0;
    uint32_t firstWordInvalidFrames = 0;
    uint32_t truncatedFrames = 0;
    CsiMotionSnapshot finalSnapshot{};
};

using CsiCaptureReplayObserver = void (*)(uint32_t frameIndex,
                                          const CsiCaptureFrame& frame,
                                          const CsiMotionSnapshot& snapshot,
                                          void* context);

namespace DETAIL {

#define CSI_CAPTURE_DECLARE_HAS_MEMBER(member)                                                   \
    template <typename T, typename = void>                                                        \
    struct Has_##member : std::false_type {};                                                     \
    template <typename T>                                                                         \
    struct Has_##member<T, std::void_t<decltype(std::declval<T&>().member)>> : std::true_type {}

CSI_CAPTURE_DECLARE_HAS_MEMBER(channel);
CSI_CAPTURE_DECLARE_HAS_MEMBER(secondary_channel);
CSI_CAPTURE_DECLARE_HAS_MEMBER(rate);
CSI_CAPTURE_DECLARE_HAS_MEMBER(sig_mode);
CSI_CAPTURE_DECLARE_HAS_MEMBER(mcs);
CSI_CAPTURE_DECLARE_HAS_MEMBER(cwb);
CSI_CAPTURE_DECLARE_HAS_MEMBER(smoothing);
CSI_CAPTURE_DECLARE_HAS_MEMBER(not_sounding);
CSI_CAPTURE_DECLARE_HAS_MEMBER(aggregation);
CSI_CAPTURE_DECLARE_HAS_MEMBER(stbc);
CSI_CAPTURE_DECLARE_HAS_MEMBER(fec_coding);
CSI_CAPTURE_DECLARE_HAS_MEMBER(sgi);
CSI_CAPTURE_DECLARE_HAS_MEMBER(ampdu_cnt);
CSI_CAPTURE_DECLARE_HAS_MEMBER(ant);
CSI_CAPTURE_DECLARE_HAS_MEMBER(noise_floor);
CSI_CAPTURE_DECLARE_HAS_MEMBER(sig_len);
CSI_CAPTURE_DECLARE_HAS_MEMBER(rx_state);

#undef CSI_CAPTURE_DECLARE_HAS_MEMBER

template <typename RxControl>
void populateOptionalRxMetadata(RxControl& rx, const CsiCaptureFrame& frame) {
    if constexpr (Has_channel<RxControl>::value) rx.channel = frame.channel;
    if constexpr (Has_secondary_channel<RxControl>::value) rx.secondary_channel = frame.secondaryChannel;
    if constexpr (Has_rate<RxControl>::value) rx.rate = frame.rate;
    if constexpr (Has_sig_mode<RxControl>::value) rx.sig_mode = frame.sigMode;
    if constexpr (Has_mcs<RxControl>::value) rx.mcs = frame.mcs;
    if constexpr (Has_cwb<RxControl>::value) rx.cwb = frame.cwb;
    if constexpr (Has_smoothing<RxControl>::value) rx.smoothing = frame.smoothing;
    if constexpr (Has_not_sounding<RxControl>::value) rx.not_sounding = frame.notSounding;
    if constexpr (Has_aggregation<RxControl>::value) rx.aggregation = frame.aggregation;
    if constexpr (Has_stbc<RxControl>::value) rx.stbc = frame.stbc;
    if constexpr (Has_fec_coding<RxControl>::value) rx.fec_coding = frame.fecCoding;
    if constexpr (Has_sgi<RxControl>::value) rx.sgi = frame.shortGuardInterval;
    if constexpr (Has_ampdu_cnt<RxControl>::value) rx.ampdu_cnt = frame.ampduCount;
    if constexpr (Has_ant<RxControl>::value) rx.ant = frame.antenna;
    if constexpr (Has_noise_floor<RxControl>::value) rx.noise_floor = frame.noiseFloor;
    if constexpr (Has_sig_len<RxControl>::value) rx.sig_len = frame.sigLength;
    if constexpr (Has_rx_state<RxControl>::value) rx.rx_state = frame.rxState;
}

inline CsiPacket makeReplayPacket(const CsiCaptureFrame& frame) {
    CsiPacket packet{};
    packet.rx_ctrl.rssi = frame.rssi;
    packet.rx_ctrl.timestamp = frame.rxTimestampUs;
    populateOptionalRxMetadata(packet.rx_ctrl, frame);
    std::memcpy(packet.mac, frame.sourceMac, sizeof(packet.mac));
    std::memcpy(packet.dmac, frame.destinationMac, sizeof(packet.dmac));
    std::memcpy(packet.buf, frame.iq.data(), frame.storedLength);
    packet.len = frame.storedLength;
    packet.originalLen = frame.originalLength;
    packet.rxSequence = frame.rxSeq;
    packet.firstWordInvalid = frame.firstWordInvalid();
    packet.acceptedSequence = frame.acceptedSeq;
    packet.processTimestampMs = frame.processNowMs;
    packet.compensate_gain = frame.compensateGain;
    return packet;
}

} // namespace DETAIL

// The caller configures the detector before replay. Observed score/motion and
// scenario.json are intentionally ignored here: only the captured production
// inputs reach the exact production CsiBandMotionDetector::process method.
inline bool replayCsiCapture(const CsiCaptureDecoder& decoder,
                             CsiBandMotionDetector& detector,
                             CsiCaptureReplayResult& result,
                             CsiCaptureReplayObserver observer = nullptr,
                             void* observerContext = nullptr) {
    result = {};
    if (!decoder.isOpen()) {
        return false;
    }

    CsiCaptureFrameCursor cursor = decoder.beginFrames();
    CsiCaptureFrame frame;
    while (decoder.nextFrame(cursor, frame)) {
        const CsiPacket packet = DETAIL::makeReplayPacket(frame);
        const CsiMotionSnapshot snapshot = detector.process(packet, frame.processNowMs);
        if (result.framesReplayed == 0) {
            result.firstAcceptedSeq = frame.acceptedSeq;
        }
        result.lastAcceptedSeq = frame.acceptedSeq;
        result.framesReplayed++;
        if (frame.firstWordInvalid()) {
            result.firstWordInvalidFrames++;
        }
        if (frame.truncated()) {
            result.truncatedFrames++;
        }
        result.finalSnapshot = snapshot;
        if (observer) {
            observer(cursor.index - 1u, frame, snapshot, observerContext);
        }
    }
    return result.framesReplayed == decoder.frameCount();
}

} // namespace NATIVE_CAPTURE
} // namespace CSI
} // namespace WIFISENSING
