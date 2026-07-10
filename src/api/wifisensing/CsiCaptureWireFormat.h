#pragma once

#include <cstddef>
#include <cstdint>

#include "../../wifisensing/csi/data/CsiTypes.h"

namespace API {
namespace CSI_CAPTURE_WIRE {

constexpr uint8_t VERSION_MAJOR = 1;
constexpr uint8_t VERSION_MINOR = 0;
constexpr size_t BATCH_HEADER_BYTES = 16;
constexpr size_t RECORD_HEADER_BYTES = 64;
constexpr size_t RECORD_MAX_BYTES =
    RECORD_HEADER_BYTES + WIFISENSING::CSI::MAX_CSI_DATA_LEN;
constexpr size_t BATCH_MAX_BYTES =
    BATCH_HEADER_BYTES +
    (RECORD_MAX_BYTES * WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS);

enum class MessageType : uint8_t {
    Hello = 1,
    Data = 2,
    End = 3,
    Error = 4,
};

enum class EndReason : uint16_t {
    ClientStop = 1,
};

enum class ErrorCode : uint16_t {
    Busy = 1,
    AlreadyStarted = 2,
    NotStarted = 3,
    UnsupportedCommand = 4,
    TransportUnavailable = 5,
};

enum SessionErrorFlag : uint32_t {
    SOURCE_METRICS_UNAVAILABLE = 1u << 0,
    MOTION_CONTROL_CHANGED = 1u << 1,
    MISSING_REPLAY_ORIGIN = 1u << 2,
    SOURCE_SEQUENCE_INCOMPLETE = 1u << 3,
};

struct HelloPayload {
    uint32_t startedAtMs = 0;
    uint32_t rxFramesStart = 0;
    uint32_t rxAcceptedStart = 0;
    uint32_t queuedPacketsStart = 0;
    uint32_t sourceQueueDropsStart = 0;
    uint32_t rxThrottleIntervalUs = 0;
    uint32_t motionControlEpoch = 0;
};

struct EndPayload {
    uint32_t stoppedAtMs = 0;
    EndReason reason = EndReason::ClientStop;
    uint32_t firstAcceptedSequence = 0;
    uint32_t lastAcceptedSequence = 0;
    uint32_t recordsOffered = 0;
    uint32_t recordsEnqueued = 0;
    uint32_t recordsDropped = 0;
    uint32_t dataBatchesOffered = 0;
    uint32_t dataBatchesEnqueued = 0;
    uint32_t dataBatchesDropped = 0;
    uint32_t truncatedRecords = 0;
    uint32_t rxFramesEnd = 0;
    uint32_t rxAcceptedEnd = 0;
    uint32_t queuedPacketsEnd = 0;
    uint32_t sourceQueueDropsEnd = 0;
    uint32_t sessionErrorFlags = 0;
};

constexpr size_t HELLO_PAYLOAD_BYTES = 40;
constexpr size_t END_PAYLOAD_BYTES = 64;
constexpr size_t ERROR_PAYLOAD_BYTES = 4;
constexpr size_t HELLO_MESSAGE_BYTES = BATCH_HEADER_BYTES + HELLO_PAYLOAD_BYTES;
constexpr size_t END_MESSAGE_BYTES = BATCH_HEADER_BYTES + END_PAYLOAD_BYTES;
constexpr size_t ERROR_MESSAGE_BYTES = BATCH_HEADER_BYTES + ERROR_PAYLOAD_BYTES;

enum RecordFlag : uint8_t {
    FIRST_WORD_INVALID = 1u << 0,
    INPUT_TRUNCATED = 1u << 1,
    OBSERVED_MOTION = 1u << 2,
    REPLAY_ORIGIN = 1u << 3,
};

constexpr uint16_t CAPABILITY_FLAGS =
    (1u << 0) | // source MAC
    (1u << 1) | // destination MAC
    (1u << 2) | // first-word-invalid marker
    (1u << 3) | // RX sequence
    (1u << 4) | // canonical PHY fields
    (1u << 5) | // observed detector output (diagnostic only)
    (1u << 6) | // accepted-sequence START/STOP fences
    (1u << 7);  // deterministic replay origin on exactly the first record

// Diagnostic capture format. Unlike the stable browser /ws/csi protocol this
// record preserves the exact detector inputs plus canonical radio metadata.
// Every integer is little-endian and wifi_pkt_rx_ctrl_t is never dumped raw.
size_t writeRecord(uint8_t* buffer,
                   size_t capacity,
                   const WIFISENSING::CSI::CsiPacket& packet,
                   bool replayOrigin = false);

size_t writeBatch(uint8_t* buffer,
                  size_t capacity,
                  uint32_t sessionId,
                  const WIFISENSING::CSI::CsiPacket* batch,
                  size_t count,
                  bool firstRecordIsReplayOrigin = false);

size_t writeHello(uint8_t* buffer,
                  size_t capacity,
                  uint32_t sessionId,
                  const HelloPayload& payload);

size_t writeEnd(uint8_t* buffer,
                size_t capacity,
                uint32_t sessionId,
                const EndPayload& payload);

size_t writeError(uint8_t* buffer,
                  size_t capacity,
                  uint32_t sessionId,
                  ErrorCode code);

} // namespace CSI_CAPTURE_WIRE
} // namespace API
