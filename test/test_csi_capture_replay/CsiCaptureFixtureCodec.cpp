#include "CsiCaptureFixtureCodec.h"

#include <cstring>
#include <limits>

namespace WIFISENSING {
namespace CSI {
namespace NATIVE_CAPTURE {
namespace {

constexpr uint8_t kMagic[] = {'M', 'H', 'C', 'F'};
constexpr uint32_t kUint32HalfRange = 0x80000000u;

uint16_t readU16(const uint8_t* value) {
    return static_cast<uint16_t>(value[0]) |
           static_cast<uint16_t>(static_cast<uint16_t>(value[1]) << 8u);
}

uint32_t readU32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8u) |
           (static_cast<uint32_t>(value[2]) << 16u) |
           (static_cast<uint32_t>(value[3]) << 24u);
}

uint64_t readU64(const uint8_t* value) {
    return static_cast<uint64_t>(readU32(value)) |
           (static_cast<uint64_t>(readU32(value + 4)) << 32u);
}

void writeU16(uint8_t* output, uint16_t value) {
    output[0] = static_cast<uint8_t>(value & 0xffu);
    output[1] = static_cast<uint8_t>((value >> 8u) & 0xffu);
}

void writeU32(uint8_t* output, uint32_t value) {
    output[0] = static_cast<uint8_t>(value & 0xffu);
    output[1] = static_cast<uint8_t>((value >> 8u) & 0xffu);
    output[2] = static_cast<uint8_t>((value >> 16u) & 0xffu);
    output[3] = static_cast<uint8_t>((value >> 24u) & 0xffu);
}

void writeU64(uint8_t* output, uint64_t value) {
    writeU32(output, static_cast<uint32_t>(value));
    writeU32(output + 4, static_cast<uint32_t>(value >> 32u));
}

uint32_t floatBits(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

float bitsToFloat(uint32_t bits) {
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

bool isBooleanByte(uint8_t value) {
    return value <= 1;
}

CsiCaptureError validateFrame(const CsiCaptureFrame& frame) {
    if (frame.storedLength == 0) {
        return CsiCaptureError::EmptyIq;
    }
    if ((frame.storedLength & 1u) != 0) {
        return CsiCaptureError::OddIqLength;
    }
    if (frame.storedLength > MAX_CSI_DATA_LEN) {
        return CsiCaptureError::IqTooLong;
    }
    if (frame.originalLength < frame.storedLength) {
        return CsiCaptureError::InvalidOriginalLength;
    }
    if ((frame.originalLength & 1u) != 0) {
        return CsiCaptureError::OddOriginalLength;
    }
    const bool lengthsShowTruncation = frame.originalLength > frame.storedLength;
    if (lengthsShowTruncation != frame.truncated()) {
        return CsiCaptureError::InvalidTruncationFlag;
    }
    if (lengthsShowTruncation) {
        return CsiCaptureError::TruncatedFrame;
    }
    if ((frame.flags & static_cast<uint8_t>(~CSI_CAPTURE_KNOWN_FRAME_FLAGS)) != 0) {
        return CsiCaptureError::InvalidFrameFlags;
    }
    if (!isBooleanByte(frame.cwb) ||
        !isBooleanByte(frame.smoothing) ||
        !isBooleanByte(frame.notSounding) ||
        !isBooleanByte(frame.aggregation) ||
        !isBooleanByte(frame.fecCoding) ||
        !isBooleanByte(frame.shortGuardInterval) ||
        !isBooleanByte(frame.antenna)) {
        return CsiCaptureError::InvalidBooleanMetadata;
    }
    return CsiCaptureError::None;
}

} // namespace

const char* toString(CsiCaptureError error) {
    switch (error) {
        case CsiCaptureError::None: return "none";
        case CsiCaptureError::NullInput: return "null_input";
        case CsiCaptureError::HeaderTruncated: return "header_truncated";
        case CsiCaptureError::BadMagic: return "bad_magic";
        case CsiCaptureError::UnsupportedVersion: return "unsupported_version";
        case CsiCaptureError::WrongByteOrder: return "wrong_byte_order";
        case CsiCaptureError::BadHeaderSize: return "bad_header_size";
        case CsiCaptureError::InvalidHeaderFlags: return "invalid_header_flags";
        case CsiCaptureError::EmptyCapture: return "empty_capture";
        case CsiCaptureError::TooManyFrames: return "too_many_frames";
        case CsiCaptureError::CaptureTooLarge: return "capture_too_large";
        case CsiCaptureError::SizeMismatch: return "size_mismatch";
        case CsiCaptureError::FrameHeaderTruncated: return "frame_header_truncated";
        case CsiCaptureError::BadFrameHeaderSize: return "bad_frame_header_size";
        case CsiCaptureError::BadFrameRecordSize: return "bad_frame_record_size";
        case CsiCaptureError::EmptyIq: return "empty_iq";
        case CsiCaptureError::OddIqLength: return "odd_iq_length";
        case CsiCaptureError::IqTooLong: return "iq_too_long";
        case CsiCaptureError::InvalidOriginalLength: return "invalid_original_length";
        case CsiCaptureError::OddOriginalLength: return "odd_original_length";
        case CsiCaptureError::InvalidTruncationFlag: return "invalid_truncation_flag";
        case CsiCaptureError::TruncatedFrame: return "truncated_frame";
        case CsiCaptureError::MissingReplayOrigin: return "missing_replay_origin";
        case CsiCaptureError::UnexpectedReplayOrigin: return "unexpected_replay_origin";
        case CsiCaptureError::InvalidFrameFlags: return "invalid_frame_flags";
        case CsiCaptureError::InvalidBooleanMetadata: return "invalid_boolean_metadata";
        case CsiCaptureError::NonZeroReserved: return "non_zero_reserved";
        case CsiCaptureError::SequenceGap: return "sequence_gap";
        case CsiCaptureError::NonMonotonicProcessTime: return "non_monotonic_process_time";
        case CsiCaptureError::FrameSectionMismatch: return "frame_section_mismatch";
        case CsiCaptureError::OutputTooSmall: return "output_too_small";
        default: return "unknown";
    }
}

bool CsiCaptureDecoder::fail(CsiCaptureError error) {
    _bytes = nullptr;
    _size = 0;
    _sessionId = 0;
    _frameCount = 0;
    _open = false;
    _error = error;
    return false;
}

bool CsiCaptureDecoder::open(const uint8_t* bytes, size_t size) {
    fail(CsiCaptureError::None);
    if (!bytes) {
        return fail(CsiCaptureError::NullInput);
    }
    if (size < CSI_CAPTURE_FILE_HEADER_BYTES) {
        return fail(CsiCaptureError::HeaderTruncated);
    }
    if (size > CSI_CAPTURE_MAX_BYTES) {
        return fail(CsiCaptureError::CaptureTooLarge);
    }
    if (std::memcmp(bytes, kMagic, sizeof(kMagic)) != 0) {
        return fail(CsiCaptureError::BadMagic);
    }
    if (bytes[4] != CSI_CAPTURE_VERSION_MAJOR || bytes[5] != CSI_CAPTURE_VERSION_MINOR) {
        return fail(CsiCaptureError::UnsupportedVersion);
    }
    if (readU32(bytes + 8) != CSI_CAPTURE_ENDIAN_TAG) {
        return fail(CsiCaptureError::WrongByteOrder);
    }
    if (readU16(bytes + 6) != CSI_CAPTURE_FILE_HEADER_BYTES ||
        readU16(bytes + 12) != CSI_CAPTURE_FRAME_HEADER_BYTES) {
        return fail(CsiCaptureError::BadHeaderSize);
    }
    if (readU16(bytes + 14) != 0) {
        return fail(CsiCaptureError::InvalidHeaderFlags);
    }

    const uint32_t frameCount = readU32(bytes + 20);
    const uint64_t frameSectionBytes = readU64(bytes + 24);
    if (frameCount == 0) {
        return fail(CsiCaptureError::EmptyCapture);
    }
    if (frameCount > CSI_CAPTURE_MAX_FRAME_COUNT) {
        return fail(CsiCaptureError::TooManyFrames);
    }
    if (frameSectionBytes > CSI_CAPTURE_MAX_BYTES - CSI_CAPTURE_FILE_HEADER_BYTES) {
        return fail(CsiCaptureError::CaptureTooLarge);
    }
    const uint64_t expectedSize = CSI_CAPTURE_FILE_HEADER_BYTES + frameSectionBytes;
    if (expectedSize != size) {
        return fail(CsiCaptureError::SizeMismatch);
    }

    const size_t frameEnd = size;
    size_t offset = CSI_CAPTURE_FILE_HEADER_BYTES;
    uint32_t previousSeq = 0;
    uint32_t previousProcessNowMs = 0;
    for (uint32_t index = 0; index < frameCount; ++index) {
        if (frameEnd - offset < CSI_CAPTURE_FRAME_HEADER_BYTES) {
            return fail(CsiCaptureError::FrameHeaderTruncated);
        }
        const uint16_t recordBytes = readU16(bytes + offset);
        const uint16_t headerBytes = readU16(bytes + offset + 2);
        const uint16_t originalLength = readU16(bytes + offset + 24);
        const uint16_t storedLength = readU16(bytes + offset + 26);
        const uint8_t flags = bytes[offset + 61];
        if (headerBytes != CSI_CAPTURE_FRAME_HEADER_BYTES) {
            return fail(CsiCaptureError::BadFrameHeaderSize);
        }
        if (storedLength == 0) {
            return fail(CsiCaptureError::EmptyIq);
        }
        if ((storedLength & 1u) != 0) {
            return fail(CsiCaptureError::OddIqLength);
        }
        if (storedLength > MAX_CSI_DATA_LEN) {
            return fail(CsiCaptureError::IqTooLong);
        }
        if (originalLength < storedLength) {
            return fail(CsiCaptureError::InvalidOriginalLength);
        }
        if ((originalLength & 1u) != 0) {
            return fail(CsiCaptureError::OddOriginalLength);
        }
        if (recordBytes != static_cast<uint16_t>(CSI_CAPTURE_FRAME_HEADER_BYTES + storedLength)) {
            return fail(CsiCaptureError::BadFrameRecordSize);
        }
        if ((originalLength > storedLength) != ((flags & Truncated) != 0)) {
            return fail(CsiCaptureError::InvalidTruncationFlag);
        }
        if (originalLength > storedLength) {
            return fail(CsiCaptureError::TruncatedFrame);
        }
        if (index == 0 && (flags & ReplayOrigin) == 0) {
            return fail(CsiCaptureError::MissingReplayOrigin);
        }
        if (index > 0 && (flags & ReplayOrigin) != 0) {
            return fail(CsiCaptureError::UnexpectedReplayOrigin);
        }
        if ((flags & static_cast<uint8_t>(~CSI_CAPTURE_KNOWN_FRAME_FLAGS)) != 0) {
            return fail(CsiCaptureError::InvalidFrameFlags);
        }
        if (!isBooleanByte(bytes[offset + 49]) ||
            !isBooleanByte(bytes[offset + 50]) ||
            !isBooleanByte(bytes[offset + 51]) ||
            !isBooleanByte(bytes[offset + 52]) ||
            !isBooleanByte(bytes[offset + 54]) ||
            !isBooleanByte(bytes[offset + 55]) ||
            !isBooleanByte(bytes[offset + 59])) {
            return fail(CsiCaptureError::InvalidBooleanMetadata);
        }
        if (bytes[offset + 62] != 0 || bytes[offset + 63] != 0) {
            return fail(CsiCaptureError::NonZeroReserved);
        }
        const uint32_t acceptedSeq = readU32(bytes + offset + 4);
        const uint32_t processNowMs = readU32(bytes + offset + 8);
        if (index > 0 && acceptedSeq != previousSeq + 1u) {
            return fail(CsiCaptureError::SequenceGap);
        }
        if (index > 0 && (processNowMs - previousProcessNowMs) >= kUint32HalfRange) {
            return fail(CsiCaptureError::NonMonotonicProcessTime);
        }
        previousSeq = acceptedSeq;
        previousProcessNowMs = processNowMs;
        if (recordBytes > frameEnd - offset) {
            return fail(CsiCaptureError::FrameSectionMismatch);
        }
        offset += recordBytes;
    }
    if (offset != frameEnd) {
        return fail(CsiCaptureError::FrameSectionMismatch);
    }

    _bytes = bytes;
    _size = size;
    _sessionId = readU32(bytes + 16);
    _frameCount = frameCount;
    _open = true;
    _error = CsiCaptureError::None;
    return true;
}

CsiCaptureFrameCursor CsiCaptureDecoder::beginFrames() const {
    return {};
}

bool CsiCaptureDecoder::hasNextFrame(const CsiCaptureFrameCursor& cursor) const {
    return _open &&
           cursor.index < _frameCount &&
           cursor.offset >= CSI_CAPTURE_FILE_HEADER_BYTES &&
           cursor.offset <= _size &&
           (_size - cursor.offset) >= CSI_CAPTURE_FRAME_HEADER_BYTES;
}

bool CsiCaptureDecoder::nextFrame(CsiCaptureFrameCursor& cursor, CsiCaptureFrame& frame) const {
    if (!hasNextFrame(cursor)) {
        return false;
    }

    const uint8_t* record = _bytes + cursor.offset;
    const uint16_t recordBytes = readU16(record);
    const uint16_t headerBytes = readU16(record + 2);
    const uint16_t storedLength = readU16(record + 26);
    if (headerBytes != CSI_CAPTURE_FRAME_HEADER_BYTES ||
        storedLength == 0 ||
        storedLength > MAX_CSI_DATA_LEN ||
        (storedLength & 1u) != 0 ||
        recordBytes != static_cast<uint16_t>(CSI_CAPTURE_FRAME_HEADER_BYTES + storedLength) ||
        recordBytes > (_size - cursor.offset)) {
        return false;
    }
    frame = {};
    frame.acceptedSeq = readU32(record + 4);
    frame.processNowMs = readU32(record + 8);
    frame.rxTimestampUs = readU32(record + 12);
    frame.compensateGain = bitsToFloat(readU32(record + 16));
    frame.observedMotionScore = bitsToFloat(readU32(record + 20));
    frame.originalLength = readU16(record + 24);
    frame.storedLength = storedLength;
    frame.rxSeq = readU16(record + 28);
    frame.sigLength = readU16(record + 30);
    std::memcpy(frame.sourceMac, record + 32, sizeof(frame.sourceMac));
    std::memcpy(frame.destinationMac, record + 38, sizeof(frame.destinationMac));
    frame.rssi = static_cast<int8_t>(record[44]);
    frame.noiseFloor = static_cast<int8_t>(record[45]);
    frame.rate = record[46];
    frame.sigMode = record[47];
    frame.mcs = record[48];
    frame.cwb = record[49];
    frame.smoothing = record[50];
    frame.notSounding = record[51];
    frame.aggregation = record[52];
    frame.stbc = record[53];
    frame.fecCoding = record[54];
    frame.shortGuardInterval = record[55];
    frame.ampduCount = record[56];
    frame.channel = record[57];
    frame.secondaryChannel = record[58];
    frame.antenna = record[59];
    frame.rxState = record[60];
    frame.flags = record[61];
    std::memcpy(frame.iq.data(), record + CSI_CAPTURE_FRAME_HEADER_BYTES, frame.storedLength);

    cursor.offset += recordBytes;
    cursor.index++;
    return true;
}

size_t encodedCsiCaptureSize(const CsiCaptureFrame* frames,
                             uint32_t frameCount,
                             CsiCaptureError& error) {
    error = CsiCaptureError::None;
    if (frameCount == 0) {
        error = CsiCaptureError::EmptyCapture;
        return 0;
    }
    if (frameCount > 0 && !frames) {
        error = CsiCaptureError::NullInput;
        return 0;
    }
    if (frameCount > CSI_CAPTURE_MAX_FRAME_COUNT) {
        error = CsiCaptureError::TooManyFrames;
        return 0;
    }

    uint64_t size = CSI_CAPTURE_FILE_HEADER_BYTES;
    uint32_t previousSeq = 0;
    uint32_t previousProcessNowMs = 0;
    for (uint32_t index = 0; index < frameCount; ++index) {
        error = validateFrame(frames[index]);
        if (error != CsiCaptureError::None) {
            return 0;
        }
        if (index > 0 && frames[index].acceptedSeq != previousSeq + 1u) {
            error = CsiCaptureError::SequenceGap;
            return 0;
        }
        if (index > 0 &&
            (frames[index].processNowMs - previousProcessNowMs) >= kUint32HalfRange) {
            error = CsiCaptureError::NonMonotonicProcessTime;
            return 0;
        }
        if (index == 0 && !frames[index].replayOrigin()) {
            error = CsiCaptureError::MissingReplayOrigin;
            return 0;
        }
        if (index > 0 && frames[index].replayOrigin()) {
            error = CsiCaptureError::UnexpectedReplayOrigin;
            return 0;
        }
        previousSeq = frames[index].acceptedSeq;
        previousProcessNowMs = frames[index].processNowMs;
        size += CSI_CAPTURE_FRAME_HEADER_BYTES + frames[index].storedLength;
    }
    if (size > CSI_CAPTURE_MAX_BYTES || size > std::numeric_limits<size_t>::max()) {
        error = CsiCaptureError::CaptureTooLarge;
        return 0;
    }
    return static_cast<size_t>(size);
}

bool encodeCsiCapture(uint32_t sessionId,
                      const CsiCaptureFrame* frames,
                      uint32_t frameCount,
                      uint8_t* output,
                      size_t outputSize,
                      size_t& bytesWritten,
                      CsiCaptureError& error) {
    bytesWritten = 0;
    const size_t encodedSize = encodedCsiCaptureSize(frames, frameCount, error);
    if (encodedSize == 0) {
        return false;
    }
    if (!output) {
        error = CsiCaptureError::NullInput;
        return false;
    }
    if (outputSize < encodedSize) {
        error = CsiCaptureError::OutputTooSmall;
        return false;
    }

    std::memcpy(output, kMagic, sizeof(kMagic));
    output[4] = CSI_CAPTURE_VERSION_MAJOR;
    output[5] = CSI_CAPTURE_VERSION_MINOR;
    writeU16(output + 6, CSI_CAPTURE_FILE_HEADER_BYTES);
    writeU32(output + 8, CSI_CAPTURE_ENDIAN_TAG);
    writeU16(output + 12, CSI_CAPTURE_FRAME_HEADER_BYTES);
    writeU16(output + 14, 0);
    writeU32(output + 16, sessionId);
    writeU32(output + 20, frameCount);
    writeU64(output + 24, encodedSize - CSI_CAPTURE_FILE_HEADER_BYTES);

    size_t offset = CSI_CAPTURE_FILE_HEADER_BYTES;
    for (uint32_t index = 0; index < frameCount; ++index) {
        const CsiCaptureFrame& frame = frames[index];
        const uint16_t recordBytes = static_cast<uint16_t>(CSI_CAPTURE_FRAME_HEADER_BYTES + frame.storedLength);
        writeU16(output + offset, recordBytes);
        writeU16(output + offset + 2, CSI_CAPTURE_FRAME_HEADER_BYTES);
        writeU32(output + offset + 4, frame.acceptedSeq);
        writeU32(output + offset + 8, frame.processNowMs);
        writeU32(output + offset + 12, frame.rxTimestampUs);
        writeU32(output + offset + 16, floatBits(frame.compensateGain));
        writeU32(output + offset + 20, floatBits(frame.observedMotionScore));
        writeU16(output + offset + 24, frame.originalLength);
        writeU16(output + offset + 26, frame.storedLength);
        writeU16(output + offset + 28, frame.rxSeq);
        writeU16(output + offset + 30, frame.sigLength);
        std::memcpy(output + offset + 32, frame.sourceMac, sizeof(frame.sourceMac));
        std::memcpy(output + offset + 38, frame.destinationMac, sizeof(frame.destinationMac));
        output[offset + 44] = static_cast<uint8_t>(frame.rssi);
        output[offset + 45] = static_cast<uint8_t>(frame.noiseFloor);
        output[offset + 46] = frame.rate;
        output[offset + 47] = frame.sigMode;
        output[offset + 48] = frame.mcs;
        output[offset + 49] = frame.cwb;
        output[offset + 50] = frame.smoothing;
        output[offset + 51] = frame.notSounding;
        output[offset + 52] = frame.aggregation;
        output[offset + 53] = frame.stbc;
        output[offset + 54] = frame.fecCoding;
        output[offset + 55] = frame.shortGuardInterval;
        output[offset + 56] = frame.ampduCount;
        output[offset + 57] = frame.channel;
        output[offset + 58] = frame.secondaryChannel;
        output[offset + 59] = frame.antenna;
        output[offset + 60] = frame.rxState;
        output[offset + 61] = frame.flags;
        output[offset + 62] = 0;
        output[offset + 63] = 0;
        std::memcpy(output + offset + CSI_CAPTURE_FRAME_HEADER_BYTES, frame.iq.data(), frame.storedLength);
        offset += recordBytes;
    }

    bytesWritten = offset;
    error = CsiCaptureError::None;
    return true;
}

} // namespace NATIVE_CAPTURE
} // namespace CSI
} // namespace WIFISENSING
