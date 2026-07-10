#pragma once

#include <cstddef>
#include <cstdint>

namespace API {
namespace CSI_CAPTURE {

// Capture sessions are short compared with the uint32_t sequence wrap period.
// Distances below half the sequence space are therefore unambiguously newer
// than the START boundary, including across one uint32_t wrap.
constexpr uint32_t SEQUENCE_HALF_RANGE = 0x80000000u;

struct BatchSlice {
    size_t offset = 0;
    size_t count = 0;
    bool reachedStopBoundary = false;
};

inline uint32_t distanceAfter(uint32_t sequence, uint32_t startExclusive) {
    return sequence - startExclusive;
}

inline bool isAfter(uint32_t sequence, uint32_t startExclusive) {
    const uint32_t distance = distanceAfter(sequence, startExclusive);
    return distance != 0 && distance < SEQUENCE_HALF_RANGE;
}

inline bool isCompleteWindow(uint32_t startExclusive,
                             uint32_t stopInclusive,
                             uint32_t recordsOffered,
                             uint32_t firstSequence,
                             uint32_t lastSequence) {
    const uint32_t expectedRecords = distanceAfter(stopInclusive, startExclusive);
    if (expectedRecords >= SEQUENCE_HALF_RANGE) {
        return false;
    }
    if (expectedRecords == 0) {
        return recordsOffered == 0;
    }
    return recordsOffered == expectedRecords &&
           firstSequence == startExclusive + 1u &&
           lastSequence == stopInclusive;
}

template <typename Packet>
inline BatchSlice selectBatchSlice(
    const Packet* batch,
    size_t count,
    uint32_t startExclusive,
    bool stopping,
    uint32_t stopInclusive) {
    BatchSlice slice;
    if (!batch || count == 0) {
        return slice;
    }

    const uint32_t stopDistance = distanceAfter(stopInclusive, startExclusive);
    if (stopping && stopDistance == 0) {
        slice.reachedStopBoundary = true;
        return slice;
    }

    bool foundFirst = false;
    for (size_t index = 0; index < count; ++index) {
        const uint32_t distance =
            distanceAfter(batch[index].acceptedSequence, startExclusive);

        // A zero or half-range-and-larger distance belongs to backlog at or
        // before the START fence. Backlog is expected only before the slice.
        if (distance == 0 || distance >= SEQUENCE_HALF_RANGE) {
            if (foundFirst) {
                break;
            }
            continue;
        }

        if (stopping && distance > stopDistance) {
            slice.reachedStopBoundary = true;
            break;
        }

        if (!foundFirst) {
            slice.offset = index;
            foundFirst = true;
        }
        ++slice.count;

        if (stopping && distance == stopDistance) {
            slice.reachedStopBoundary = true;
            break;
        }
    }

    return slice;
}

} // namespace CSI_CAPTURE
} // namespace API
