#include "CsiWireFormat.h"

#include <cstring>

namespace API {
namespace CSI_WIRE {

namespace {

uint8_t encodeGain(float compensateGain) {
    const float gainTimesTen = compensateGain * 10.0f;
    if (gainTimesTen <= 0.0f) {
        return 0;
    }
    if (gainTimesTen >= 255.0f) {
        return 255;
    }
    return static_cast<uint8_t>(gainTimesTen);
}

} // namespace

size_t writeRecord(uint8_t* buffer,
                   size_t capacity,
                   const WIFISENSING::CSI::CsiPacket& packet) {
    if (!buffer) {
        return 0;
    }

    uint16_t dataLen = static_cast<uint16_t>(packet.len);
    if (dataLen > WIFISENSING::CSI::MAX_CSI_DATA_LEN) {
        dataLen = WIFISENSING::CSI::MAX_CSI_DATA_LEN;
    }

    const size_t required = RECORD_HEADER_BYTES + dataLen;
    if (dataLen == 0 || capacity < required) {
        return 0;
    }

    size_t offset = 0;
    const uint32_t timestamp = packet.rx_ctrl.timestamp;
    memcpy(&buffer[offset], &timestamp, sizeof(timestamp));
    offset += sizeof(timestamp);

    buffer[offset++] = static_cast<uint8_t>(packet.rx_ctrl.rssi);

    memcpy(&buffer[offset], &dataLen, sizeof(dataLen));
    offset += sizeof(dataLen);

    buffer[offset++] = encodeGain(packet.compensate_gain);

    memcpy(&buffer[offset], &packet.motionScore, sizeof(packet.motionScore));
    offset += sizeof(packet.motionScore);

    buffer[offset++] = packet.isMotionDetected ? 1 : 0;

    memcpy(&buffer[offset], packet.buf, dataLen);
    offset += dataLen;
    return offset;
}

size_t writeBatch(uint8_t* buffer,
                  size_t capacity,
                  const WIFISENSING::CSI::CsiPacket* batch,
                  size_t count) {
    if (!buffer || !batch || count == 0) {
        return 0;
    }

    const size_t cappedCount =
        (count > WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS)
            ? WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS
            : count;

    size_t offset = 0;
    for (size_t i = 0; i < cappedCount; ++i) {
        const size_t written = writeRecord(buffer + offset, capacity - offset, batch[i]);
        if (written == 0) {
            return 0;
        }
        offset += written;
    }

    return offset;
}

} // namespace CSI_WIRE
} // namespace API
