#include "CsiCaptureWireFormat.h"

#include <cstring>

namespace API {
namespace CSI_CAPTURE_WIRE {
namespace {

constexpr uint8_t kBatchMagic[4] = {'M', 'H', 'C', 'B'};

void writeU16(uint8_t* buffer, size_t& offset, uint16_t value) {
    buffer[offset++] = static_cast<uint8_t>(value & 0xffu);
    buffer[offset++] = static_cast<uint8_t>((value >> 8) & 0xffu);
}

void writeU32(uint8_t* buffer, size_t& offset, uint32_t value) {
    buffer[offset++] = static_cast<uint8_t>(value & 0xffu);
    buffer[offset++] = static_cast<uint8_t>((value >> 8) & 0xffu);
    buffer[offset++] = static_cast<uint8_t>((value >> 16) & 0xffu);
    buffer[offset++] = static_cast<uint8_t>((value >> 24) & 0xffu);
}

void writeFloat32(uint8_t* buffer, size_t& offset, float value) {
    static_assert(sizeof(float) == sizeof(uint32_t), "CSI capture requires IEEE-754 float32");
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    writeU32(buffer, offset, bits);
}

uint16_t storedLength(const WIFISENSING::CSI::CsiPacket& packet) {
    return static_cast<uint16_t>(
        packet.len > WIFISENSING::CSI::MAX_CSI_DATA_LEN
            ? WIFISENSING::CSI::MAX_CSI_DATA_LEN
            : packet.len);
}

size_t writeMessageHeader(uint8_t* buffer,
                          size_t capacity,
                          MessageType type,
                          uint16_t recordHeaderSize,
                          uint16_t recordCount,
                          uint32_t sessionId) {
    if (!buffer || capacity < BATCH_HEADER_BYTES) {
        return 0;
    }

    size_t offset = 0;
    memcpy(buffer + offset, kBatchMagic, sizeof(kBatchMagic));
    offset += sizeof(kBatchMagic);
    buffer[offset++] = VERSION_MAJOR;
    buffer[offset++] = VERSION_MINOR;
    buffer[offset++] = static_cast<uint8_t>(type);
    buffer[offset++] = static_cast<uint8_t>(BATCH_HEADER_BYTES);
    writeU16(buffer, offset, recordHeaderSize);
    writeU16(buffer, offset, recordCount);
    writeU32(buffer, offset, sessionId);
    return offset;
}

} // namespace

size_t writeRecord(uint8_t* buffer,
                   size_t capacity,
                   const WIFISENSING::CSI::CsiPacket& packet,
                   bool replayOrigin) {
    if (!buffer) {
        return 0;
    }

    const uint16_t dataLen = storedLength(packet);
    const size_t sourceLength = packet.originalLen == 0 ? packet.len : packet.originalLen;
    if (sourceLength > UINT16_MAX) {
        return 0;
    }
    const uint16_t originalLen = static_cast<uint16_t>(sourceLength);
    const size_t required = RECORD_HEADER_BYTES + dataLen;
    if (dataLen == 0 || (dataLen & 1u) != 0 ||
        originalLen < dataLen || (originalLen & 1u) != 0 ||
        required > UINT16_MAX || capacity < required) {
        return 0;
    }

    size_t offset = 0;
    writeU16(buffer, offset, static_cast<uint16_t>(required));
    writeU16(buffer, offset, static_cast<uint16_t>(RECORD_HEADER_BYTES));
    writeU32(buffer, offset, packet.acceptedSequence);
    writeU32(buffer, offset, packet.processTimestampMs);
    writeU32(buffer, offset, packet.rx_ctrl.timestamp);
    writeFloat32(buffer, offset, packet.compensate_gain);
    writeFloat32(buffer, offset, packet.motionScore);
    writeU16(buffer, offset, originalLen);
    writeU16(buffer, offset, dataLen);
    writeU16(buffer, offset, packet.rxSequence);
    writeU16(buffer, offset, static_cast<uint16_t>(packet.rx_ctrl.sig_len));

    memcpy(buffer + offset, packet.mac, sizeof(packet.mac));
    offset += sizeof(packet.mac);
    memcpy(buffer + offset, packet.dmac, sizeof(packet.dmac));
    offset += sizeof(packet.dmac);

    buffer[offset++] = static_cast<uint8_t>(static_cast<int8_t>(packet.rx_ctrl.rssi));
    buffer[offset++] = static_cast<uint8_t>(static_cast<int8_t>(packet.rx_ctrl.noise_floor));
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.rate);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.sig_mode);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.mcs);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.cwb);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.smoothing);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.not_sounding);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.aggregation);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.stbc);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.fec_coding);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.sgi);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.ampdu_cnt);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.channel);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.secondary_channel);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.ant);
    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.rx_state);

    uint8_t flags = 0;
    if (packet.firstWordInvalid) {
        flags |= FIRST_WORD_INVALID;
    }
    if (originalLen > dataLen) {
        flags |= INPUT_TRUNCATED;
    }
    if (packet.isMotionDetected) {
        flags |= OBSERVED_MOTION;
    }
    if (replayOrigin) {
        flags |= REPLAY_ORIGIN;
    }
    buffer[offset++] = flags;
    writeU16(buffer, offset, 0);

    if (offset != RECORD_HEADER_BYTES) {
        return 0;
    }

    memcpy(buffer + offset, packet.buf, dataLen);
    return offset + dataLen;
}

size_t writeBatch(uint8_t* buffer,
                  size_t capacity,
                  uint32_t sessionId,
                  const WIFISENSING::CSI::CsiPacket* batch,
                  size_t count,
                  bool firstRecordIsReplayOrigin) {
    if (!buffer || !batch || count == 0 || capacity < BATCH_HEADER_BYTES) {
        return 0;
    }

    if (count > WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS) {
        return 0;
    }
    const size_t cappedCount = count;

    size_t offset = writeMessageHeader(
        buffer,
        capacity,
        MessageType::Data,
        static_cast<uint16_t>(RECORD_HEADER_BYTES),
        static_cast<uint16_t>(cappedCount),
        sessionId);
    if (offset == 0) {
        return 0;
    }

    for (size_t i = 0; i < cappedCount; ++i) {
        const size_t written = writeRecord(
            buffer + offset,
            capacity - offset,
            batch[i],
            firstRecordIsReplayOrigin && i == 0);
        if (written == 0) {
            return 0;
        }
        offset += written;
    }

    return offset;
}

size_t writeHello(uint8_t* buffer,
                  size_t capacity,
                  uint32_t sessionId,
                  const HelloPayload& payload) {
    if (capacity < HELLO_MESSAGE_BYTES) {
        return 0;
    }

    size_t offset = writeMessageHeader(
        buffer,
        capacity,
        MessageType::Hello,
        static_cast<uint16_t>(RECORD_HEADER_BYTES),
        0,
        sessionId);
    if (offset == 0) {
        return 0;
    }
    writeU32(buffer, offset, payload.startedAtMs);
    writeU32(buffer, offset, payload.rxFramesStart);
    writeU32(buffer, offset, payload.rxAcceptedStart);
    writeU32(buffer, offset, payload.queuedPacketsStart);
    writeU32(buffer, offset, payload.sourceQueueDropsStart);
    writeU32(buffer, offset, payload.rxThrottleIntervalUs);
    writeU16(buffer, offset, static_cast<uint16_t>(WIFISENSING::CSI::MAX_CSI_DATA_LEN));
    writeU16(buffer, offset, static_cast<uint16_t>(WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS));
    writeU16(buffer, offset, static_cast<uint16_t>(RECORD_HEADER_BYTES));
    writeU16(buffer, offset, CAPABILITY_FLAGS);
    writeU32(buffer, offset, 0);
    writeU32(buffer, offset, payload.motionControlEpoch);
    return offset;
}

size_t writeEnd(uint8_t* buffer,
                size_t capacity,
                uint32_t sessionId,
                const EndPayload& payload) {
    if (capacity < END_MESSAGE_BYTES) {
        return 0;
    }

    size_t offset = writeMessageHeader(
        buffer,
        capacity,
        MessageType::End,
        static_cast<uint16_t>(RECORD_HEADER_BYTES),
        0,
        sessionId);
    if (offset == 0) {
        return 0;
    }
    writeU32(buffer, offset, payload.stoppedAtMs);
    writeU32(buffer, offset, static_cast<uint32_t>(payload.reason));
    writeU32(buffer, offset, payload.firstAcceptedSequence);
    writeU32(buffer, offset, payload.lastAcceptedSequence);
    writeU32(buffer, offset, payload.recordsOffered);
    writeU32(buffer, offset, payload.recordsEnqueued);
    writeU32(buffer, offset, payload.recordsDropped);
    writeU32(buffer, offset, payload.dataBatchesOffered);
    writeU32(buffer, offset, payload.dataBatchesEnqueued);
    writeU32(buffer, offset, payload.dataBatchesDropped);
    writeU32(buffer, offset, payload.truncatedRecords);
    writeU32(buffer, offset, payload.rxFramesEnd);
    writeU32(buffer, offset, payload.rxAcceptedEnd);
    writeU32(buffer, offset, payload.queuedPacketsEnd);
    writeU32(buffer, offset, payload.sourceQueueDropsEnd);
    writeU32(buffer, offset, payload.sessionErrorFlags);
    return offset;
}

size_t writeError(uint8_t* buffer,
                  size_t capacity,
                  uint32_t sessionId,
                  ErrorCode code) {
    if (capacity < ERROR_MESSAGE_BYTES) {
        return 0;
    }

    size_t offset = writeMessageHeader(
        buffer,
        capacity,
        MessageType::Error,
        static_cast<uint16_t>(RECORD_HEADER_BYTES),
        0,
        sessionId);
    if (offset == 0) {
        return 0;
    }
    writeU16(buffer, offset, static_cast<uint16_t>(code));
    writeU16(buffer, offset, 0);
    return offset;
}

} // namespace CSI_CAPTURE_WIRE
} // namespace API
