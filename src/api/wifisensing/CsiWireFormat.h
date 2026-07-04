#pragma once

#include <cstddef>
#include <cstdint>

#include "../../wifisensing/csi/data/CsiTypes.h"

namespace API {
namespace CSI_WIRE {

constexpr size_t RECORD_HEADER_BYTES = 13;
constexpr size_t RECORD_MAX_BYTES =
    RECORD_HEADER_BYTES + WIFISENSING::CSI::MAX_CSI_DATA_LEN;
constexpr size_t BATCH_MAX_BYTES =
    RECORD_MAX_BYTES * WIFISENSING::CSI::MAX_CSI_BATCH_PACKETS;

size_t writeRecord(uint8_t* buffer,
                   size_t capacity,
                   const WIFISENSING::CSI::CsiPacket& packet);

size_t writeBatch(uint8_t* buffer,
                  size_t capacity,
                  const WIFISENSING::CSI::CsiPacket* batch,
                  size_t count);

} // namespace CSI_WIRE
} // namespace API
