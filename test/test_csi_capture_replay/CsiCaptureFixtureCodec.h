#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "../../src/wifisensing/csi/data/CsiTypes.h"
#include "../../src/api/wifisensing/CsiCaptureWireFormat.h"

namespace WIFISENSING {
namespace CSI {
namespace NATIVE_CAPTURE {

// MatrixHub CSI Fixture (MHCF) is the canonical little-endian, lossless host
// representation of frames received from /ws/csi-capture/v1. Scenario ground
// truth belongs in a separate scenario.json sidecar and is never mixed into a
// raw frame or used as input to the detector.
constexpr uint8_t CSI_CAPTURE_VERSION_MAJOR = 1;
constexpr uint8_t CSI_CAPTURE_VERSION_MINOR = 0;
constexpr uint32_t CSI_CAPTURE_ENDIAN_TAG = 0x01020304u;
constexpr size_t CSI_CAPTURE_FILE_HEADER_BYTES = 32;
constexpr size_t CSI_CAPTURE_FRAME_HEADER_BYTES = 64;
constexpr uint32_t CSI_CAPTURE_MAX_FRAME_COUNT = 1000000;
constexpr size_t CSI_CAPTURE_MAX_BYTES = 600u * 1024u * 1024u;

static_assert(MAX_CSI_DATA_LEN <= UINT16_MAX, "MHCF v1 stores CSI IQ length as uint16");
static_assert(sizeof(float) == sizeof(uint32_t), "MHCF v1 requires a 32-bit float");
static_assert(std::numeric_limits<float>::is_iec559, "MHCF v1 requires IEEE-754 float32");
static_assert(CSI_CAPTURE_VERSION_MAJOR == API::CSI_CAPTURE_WIRE::VERSION_MAJOR,
              "MHCF and capture WebSocket major versions must match");
static_assert(CSI_CAPTURE_VERSION_MINOR == API::CSI_CAPTURE_WIRE::VERSION_MINOR,
              "MHCF and capture WebSocket minor versions must match");
static_assert(CSI_CAPTURE_FRAME_HEADER_BYTES == API::CSI_CAPTURE_WIRE::RECORD_HEADER_BYTES,
              "MHCF must embed canonical capture WebSocket records unchanged");

enum class CsiCaptureError : uint8_t {
    None = 0,
    NullInput,
    HeaderTruncated,
    BadMagic,
    UnsupportedVersion,
    WrongByteOrder,
    BadHeaderSize,
    InvalidHeaderFlags,
    EmptyCapture,
    TooManyFrames,
    CaptureTooLarge,
    SizeMismatch,
    FrameHeaderTruncated,
    BadFrameHeaderSize,
    BadFrameRecordSize,
    EmptyIq,
    OddIqLength,
    IqTooLong,
    InvalidOriginalLength,
    OddOriginalLength,
    InvalidTruncationFlag,
    TruncatedFrame,
    MissingReplayOrigin,
    UnexpectedReplayOrigin,
    InvalidFrameFlags,
    InvalidBooleanMetadata,
    NonZeroReserved,
    SequenceGap,
    NonMonotonicProcessTime,
    FrameSectionMismatch,
    OutputTooSmall,
};

const char* toString(CsiCaptureError error);

enum CsiCaptureFrameFlag : uint8_t {
    FirstWordInvalid = 1u << 0,
    Truncated = 1u << 1,
    ObservedMotion = 1u << 2,
    ReplayOrigin = 1u << 3,
};

constexpr uint8_t CSI_CAPTURE_KNOWN_FRAME_FLAGS =
    FirstWordInvalid | Truncated | ObservedMotion | ReplayOrigin;

struct CsiCaptureFrame {
    uint32_t acceptedSeq = 0;
    uint32_t processNowMs = 0;
    uint32_t rxTimestampUs = 0;
    float compensateGain = 1.0f;

    // Observed firmware output is parity evidence only. The native replay must
    // never feed it into the detector or treat it as ground truth.
    float observedMotionScore = 0.0f;

    uint16_t originalLength = 0;
    uint16_t storedLength = 0;
    uint16_t rxSeq = 0;
    uint16_t sigLength = 0;
    uint8_t sourceMac[6]{};
    uint8_t destinationMac[6]{};
    int8_t rssi = 0;
    int8_t noiseFloor = 0;
    uint8_t rate = 0;
    uint8_t sigMode = 0;
    uint8_t mcs = 0;
    uint8_t cwb = 0;
    uint8_t smoothing = 0;
    uint8_t notSounding = 0;
    uint8_t aggregation = 0;
    uint8_t stbc = 0;
    uint8_t fecCoding = 0;
    uint8_t shortGuardInterval = 0;
    uint8_t ampduCount = 0;
    uint8_t channel = 0;
    uint8_t secondaryChannel = 0;
    uint8_t antenna = 0;
    uint8_t rxState = 0;
    uint8_t flags = 0;
    std::array<int8_t, MAX_CSI_DATA_LEN> iq{};

    bool firstWordInvalid() const { return (flags & FirstWordInvalid) != 0; }
    bool truncated() const { return (flags & Truncated) != 0; }
    bool observedMotion() const { return (flags & ObservedMotion) != 0; }
    bool replayOrigin() const { return (flags & ReplayOrigin) != 0; }
};

struct CsiCaptureFrameCursor {
    uint32_t index = 0;
    size_t offset = CSI_CAPTURE_FILE_HEADER_BYTES;
};

class CsiCaptureDecoder {
public:
    bool open(const uint8_t* bytes, size_t size);

    bool isOpen() const { return _open; }
    CsiCaptureError error() const { return _error; }
    uint32_t sessionId() const { return _sessionId; }
    uint32_t frameCount() const { return _frameCount; }

    CsiCaptureFrameCursor beginFrames() const;
    bool hasNextFrame(const CsiCaptureFrameCursor& cursor) const;
    bool nextFrame(CsiCaptureFrameCursor& cursor, CsiCaptureFrame& frame) const;

private:
    bool fail(CsiCaptureError error);

    const uint8_t* _bytes = nullptr;
    size_t _size = 0;
    uint32_t _sessionId = 0;
    uint32_t _frameCount = 0;
    bool _open = false;
    CsiCaptureError _error = CsiCaptureError::None;
};

size_t encodedCsiCaptureSize(const CsiCaptureFrame* frames,
                             uint32_t frameCount,
                             CsiCaptureError& error);

bool encodeCsiCapture(uint32_t sessionId,
                      const CsiCaptureFrame* frames,
                      uint32_t frameCount,
                      uint8_t* output,
                      size_t outputSize,
                      size_t& bytesWritten,
                      CsiCaptureError& error);

} // namespace NATIVE_CAPTURE
} // namespace CSI
} // namespace WIFISENSING
