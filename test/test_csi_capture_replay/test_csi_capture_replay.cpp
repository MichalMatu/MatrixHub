#include <unity.h>

#include <array>
#include <cstring>

#include "CsiCaptureFixtureCodec.h"
#include "CsiCaptureReplay.h"
#include "CsiScenarioFixtureRunnerTests.h"
#include "TinyCsiCaptureV1.h"

#include "../../src/wifisensing/csi/algo/CsiBandMotionDetector.cpp"

using namespace WIFISENSING::CSI;
using namespace WIFISENSING::CSI::NATIVE_CAPTURE;

namespace {

constexpr size_t kTestBufferBytes = 2048;

uint32_t bitsOf(float value) {
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

CsiCaptureFrame makeFrame(uint32_t acceptedSeq,
                          int8_t amplitude = 10,
                          bool replayOrigin = true) {
    CsiCaptureFrame frame;
    frame.acceptedSeq = acceptedSeq;
    frame.processNowMs = 123456789u + acceptedSeq;
    frame.rxTimestampUs = 0xf1020304u + acceptedSeq;
    frame.compensateGain = 1.234567f;
    frame.observedMotionScore = 91.125f;
    frame.originalLength = 16;
    frame.storedLength = 16;
    frame.rxSeq = 0x4567;
    frame.sigLength = 256;
    const uint8_t source[] = {0x02, 0x10, 0x20, 0x30, 0x40, 0x50};
    const uint8_t destination[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    std::memcpy(frame.sourceMac, source, sizeof(source));
    std::memcpy(frame.destinationMac, destination, sizeof(destination));
    frame.rssi = -67;
    frame.noiseFloor = -96;
    frame.rate = 7;
    frame.sigMode = 1;
    frame.mcs = 5;
    frame.cwb = 1;
    frame.smoothing = 1;
    frame.notSounding = 1;
    frame.aggregation = 1;
    frame.stbc = 2;
    frame.fecCoding = 1;
    frame.shortGuardInterval = 1;
    frame.ampduCount = 3;
    frame.channel = 6;
    frame.secondaryChannel = 1;
    frame.antenna = 1;
    frame.rxState = 4;
    frame.flags = FirstWordInvalid | ObservedMotion;
    if (replayOrigin) {
        frame.flags |= ReplayOrigin;
    }
    for (uint16_t index = 0; index < frame.storedLength; index += 2) {
        frame.iq[index] = amplitude;
        frame.iq[index + 1] = static_cast<int8_t>(index / 2);
    }
    return frame;
}

CsiMotionConfig replayConfig() {
    CsiMotionConfig config;
    config.enabled = true;
    config.bandCount = 1;
    config.bands[0] = {0, 7};
    config.baselineFrames = 30;
    config.topK = 4;
    config.autoRecalibration = false;
    return config;
}

template <size_t N>
std::array<uint8_t, N> copyTinyFixture() {
    std::array<uint8_t, N> copy{};
    std::memcpy(copy.data(), FIXTURES::TINY_CSI_CAPTURE_V1, N);
    return copy;
}

} // namespace

void setUp(void) {
    WIFISENSING::CSI::TEST_HOOKS::setCsiMotionStorageAllocationFailure(false);
}

void tearDown(void) {
    WIFISENSING::CSI::TEST_HOOKS::setCsiMotionStorageAllocationFailure(false);
}

void test_codec_round_trip_preserves_lossless_frame_metadata_and_iq() {
    CsiCaptureFrame frames[] = {
        makeFrame(0xfffffffeu),
        makeFrame(0xffffffffu, 12, false),
    };
    CsiCaptureError error = CsiCaptureError::None;
    const size_t expectedSize = encodedCsiCaptureSize(frames, 2, error);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(CsiCaptureError::None), static_cast<uint8_t>(error));
    TEST_ASSERT_EQUAL_UINT32(
        CSI_CAPTURE_FILE_HEADER_BYTES + (2 * CSI_CAPTURE_FRAME_HEADER_BYTES) + 32,
        expectedSize);

    std::array<uint8_t, kTestBufferBytes> encoded{};
    size_t bytesWritten = 0;
    TEST_ASSERT_TRUE(encodeCsiCapture(
        0x89abcdefu, frames, 2, encoded.data(), encoded.size(), bytesWritten, error));
    TEST_ASSERT_EQUAL_UINT32(expectedSize, bytesWritten);

    CsiCaptureDecoder decoder;
    TEST_ASSERT_TRUE(decoder.open(encoded.data(), bytesWritten));
    TEST_ASSERT_EQUAL_HEX32(0x89abcdefu, decoder.sessionId());
    TEST_ASSERT_EQUAL_UINT32(2, decoder.frameCount());

    CsiCaptureFrameCursor cursor = decoder.beginFrames();
    CsiCaptureFrame decoded;
    TEST_ASSERT_TRUE(decoder.nextFrame(cursor, decoded));
    TEST_ASSERT_EQUAL_HEX32(frames[0].acceptedSeq, decoded.acceptedSeq);
    TEST_ASSERT_EQUAL_UINT32(frames[0].processNowMs, decoded.processNowMs);
    TEST_ASSERT_EQUAL_HEX32(frames[0].rxTimestampUs, decoded.rxTimestampUs);
    TEST_ASSERT_EQUAL_HEX32(bitsOf(frames[0].compensateGain), bitsOf(decoded.compensateGain));
    TEST_ASSERT_EQUAL_HEX32(bitsOf(frames[0].observedMotionScore), bitsOf(decoded.observedMotionScore));
    TEST_ASSERT_EQUAL_UINT16(frames[0].originalLength, decoded.originalLength);
    TEST_ASSERT_EQUAL_UINT16(frames[0].storedLength, decoded.storedLength);
    TEST_ASSERT_EQUAL_UINT16(frames[0].rxSeq, decoded.rxSeq);
    TEST_ASSERT_EQUAL_UINT16(frames[0].sigLength, decoded.sigLength);
    TEST_ASSERT_EQUAL_MEMORY(frames[0].sourceMac, decoded.sourceMac, sizeof(decoded.sourceMac));
    TEST_ASSERT_EQUAL_MEMORY(frames[0].destinationMac, decoded.destinationMac, sizeof(decoded.destinationMac));
    TEST_ASSERT_EQUAL_INT8(frames[0].rssi, decoded.rssi);
    TEST_ASSERT_EQUAL_INT8(frames[0].noiseFloor, decoded.noiseFloor);
    TEST_ASSERT_EQUAL_UINT8(frames[0].channel, decoded.channel);
    TEST_ASSERT_EQUAL_UINT8(frames[0].sigMode, decoded.sigMode);
    TEST_ASSERT_EQUAL_UINT8(frames[0].mcs, decoded.mcs);
    TEST_ASSERT_EQUAL_UINT8(frames[0].flags, decoded.flags);
    TEST_ASSERT_EQUAL_MEMORY(frames[0].iq.data(), decoded.iq.data(), decoded.storedLength);

    TEST_ASSERT_TRUE(decoder.nextFrame(cursor, decoded));
    TEST_ASSERT_FALSE(decoded.truncated());
    TEST_ASSERT_EQUAL_UINT16(16, decoded.originalLength);
    TEST_ASSERT_FALSE(decoder.hasNextFrame(cursor));
    TEST_ASSERT_FALSE(decoder.nextFrame(cursor, decoded));
}

void test_hand_authored_embedded_fixture_decodes_with_canonical_offsets() {
    CsiCaptureDecoder decoder;
    TEST_ASSERT_TRUE(decoder.open(
        FIXTURES::TINY_CSI_CAPTURE_V1,
        FIXTURES::TINY_CSI_CAPTURE_V1_SIZE));
    TEST_ASSERT_EQUAL_HEX32(0xa1b2c3d4u, decoder.sessionId());
    TEST_ASSERT_EQUAL_UINT32(2, decoder.frameCount());

    CsiCaptureFrameCursor cursor = decoder.beginFrames();
    CsiCaptureFrame first;
    TEST_ASSERT_TRUE(decoder.nextFrame(cursor, first));
    TEST_ASSERT_EQUAL_UINT32(40, first.acceptedSeq);
    TEST_ASSERT_EQUAL_UINT32(1000, first.processNowMs);
    TEST_ASSERT_EQUAL_HEX32(0x3fa00000u, bitsOf(first.compensateGain));
    TEST_ASSERT_TRUE(first.firstWordInvalid());
    TEST_ASSERT_EQUAL_INT8(-61, first.rssi);
    TEST_ASSERT_EQUAL_UINT8(6, first.channel);
    TEST_ASSERT_EQUAL_UINT8(1, first.sigMode);
    TEST_ASSERT_EQUAL_UINT16(16, first.storedLength);

    CsiCaptureFrame second;
    TEST_ASSERT_TRUE(decoder.nextFrame(cursor, second));
    TEST_ASSERT_EQUAL_UINT32(41, second.acceptedSeq);
    TEST_ASSERT_EQUAL_UINT32(1100, second.processNowMs);
    TEST_ASSERT_EQUAL_HEX32(0x3fb00000u, bitsOf(second.compensateGain));
    TEST_ASSERT_EQUAL_FLOAT(99.0f, second.observedMotionScore);
    TEST_ASSERT_TRUE(second.observedMotion());
    TEST_ASSERT_EQUAL_INT8(12, second.iq[0]);
    TEST_ASSERT_EQUAL_INT8(1, second.iq[1]);
}

void test_replay_invokes_exact_production_detector_and_ignores_observed_output() {
    CsiCaptureDecoder decoder;
    TEST_ASSERT_TRUE(decoder.open(
        FIXTURES::TINY_CSI_CAPTURE_V1,
        FIXTURES::TINY_CSI_CAPTURE_V1_SIZE));

    CsiBandMotionDetector detector;
    TEST_ASSERT_TRUE(detector.begin());
    detector.configure(replayConfig());

    CsiCaptureReplayResult result;
    TEST_ASSERT_TRUE(replayCsiCapture(decoder, detector, result));
    TEST_ASSERT_EQUAL_UINT32(2, result.framesReplayed);
    TEST_ASSERT_EQUAL_UINT32(40, result.firstAcceptedSeq);
    TEST_ASSERT_EQUAL_UINT32(41, result.lastAcceptedSeq);
    TEST_ASSERT_EQUAL_UINT32(1, result.firstWordInvalidFrames);
    TEST_ASSERT_EQUAL_UINT32(0, result.truncatedFrames);
    TEST_ASSERT_EQUAL_UINT32(2, result.finalSnapshot.framesSeen);
    TEST_ASSERT_EQUAL_UINT16(8, result.finalSnapshot.width);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiMotionState::Calibrating),
        static_cast<uint8_t>(result.finalSnapshot.state));
    TEST_ASSERT_FALSE(result.finalSnapshot.motion);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result.finalSnapshot.score);
}

void test_decoder_rejects_unsupported_version_and_wrong_endian_marker() {
    auto unsupported = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    unsupported[4] = 2;
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(unsupported.data(), unsupported.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::UnsupportedVersion),
        static_cast<uint8_t>(decoder.error()));

    auto wrongEndian = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    wrongEndian[8] = 0x01;
    wrongEndian[9] = 0x02;
    wrongEndian[10] = 0x03;
    wrongEndian[11] = 0x04;
    TEST_ASSERT_FALSE(decoder.open(wrongEndian.data(), wrongEndian.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::WrongByteOrder),
        static_cast<uint8_t>(decoder.error()));
}

void test_codec_rejects_empty_capture() {
    uint8_t emptyCapture[CSI_CAPTURE_FILE_HEADER_BYTES] = {
        'M', 'H', 'C', 'F', 1, 0, 32, 0,
        4, 3, 2, 1, 64, 0, 0, 0,
    };
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(emptyCapture, sizeof(emptyCapture)));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::EmptyCapture),
        static_cast<uint8_t>(decoder.error()));

    CsiCaptureError error = CsiCaptureError::None;
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(nullptr, 0, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::EmptyCapture),
        static_cast<uint8_t>(error));
}

void test_decoder_rejects_truncated_header_payload_and_trailing_bytes() {
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(FIXTURES::TINY_CSI_CAPTURE_V1, 31));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::HeaderTruncated),
        static_cast<uint8_t>(decoder.error()));

    TEST_ASSERT_FALSE(decoder.open(
        FIXTURES::TINY_CSI_CAPTURE_V1,
        FIXTURES::TINY_CSI_CAPTURE_V1_SIZE - 1));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::SizeMismatch),
        static_cast<uint8_t>(decoder.error()));

    std::array<uint8_t, FIXTURES::TINY_CSI_CAPTURE_V1_SIZE + 1> trailing{};
    std::memcpy(trailing.data(), FIXTURES::TINY_CSI_CAPTURE_V1, FIXTURES::TINY_CSI_CAPTURE_V1_SIZE);
    TEST_ASSERT_FALSE(decoder.open(trailing.data(), trailing.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::SizeMismatch),
        static_cast<uint8_t>(decoder.error()));
}

void test_decoder_rejects_oversized_u64_section_without_integer_wrap() {
    auto oversized = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    for (size_t offset = 24; offset < 32; ++offset) {
        oversized[offset] = 0xff;
    }
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(oversized.data(), oversized.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::CaptureTooLarge),
        static_cast<uint8_t>(decoder.error()));
}

void test_decoder_rejects_malformed_record_lengths_flags_and_reserved_bytes() {
    CsiCaptureDecoder decoder;

    auto badRecordSize = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    badRecordSize[32] = 0x4f;
    TEST_ASSERT_FALSE(decoder.open(badRecordSize.data(), badRecordSize.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::BadFrameRecordSize),
        static_cast<uint8_t>(decoder.error()));

    auto oddIq = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    oddIq[32] = 0x4f;
    oddIq[32 + 26] = 0x0f;
    TEST_ASSERT_FALSE(decoder.open(oddIq.data(), oddIq.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::OddIqLength),
        static_cast<uint8_t>(decoder.error()));

    auto invalidFlags = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    invalidFlags[32 + 61] |= 0x80;
    TEST_ASSERT_FALSE(decoder.open(invalidFlags.data(), invalidFlags.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::InvalidFrameFlags),
        static_cast<uint8_t>(decoder.error()));

    auto nonZeroReserved = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    nonZeroReserved[32 + 62] = 1;
    TEST_ASSERT_FALSE(decoder.open(nonZeroReserved.data(), nonZeroReserved.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::NonZeroReserved),
        static_cast<uint8_t>(decoder.error()));
}

void test_decoder_and_encoder_require_exactly_one_initial_replay_origin() {
    auto missing = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    missing[32 + 61] &= static_cast<uint8_t>(~ReplayOrigin);
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(missing.data(), missing.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::MissingReplayOrigin),
        static_cast<uint8_t>(decoder.error()));

    constexpr size_t secondFrameOffset = 32 + 80;
    auto repeated = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    repeated[secondFrameOffset + 61] |= ReplayOrigin;
    TEST_ASSERT_FALSE(decoder.open(repeated.data(), repeated.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::UnexpectedReplayOrigin),
        static_cast<uint8_t>(decoder.error()));

    CsiCaptureFrame missingFrames[] = {makeFrame(1, 10, false)};
    CsiCaptureError error = CsiCaptureError::None;
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(missingFrames, 1, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::MissingReplayOrigin),
        static_cast<uint8_t>(error));

    CsiCaptureFrame repeatedFrames[] = {makeFrame(1), makeFrame(2)};
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(repeatedFrames, 2, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::UnexpectedReplayOrigin),
        static_cast<uint8_t>(error));
}

void test_decoder_and_encoder_reject_sequence_gaps() {
    auto gap = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    constexpr size_t secondFrameOffset = 32 + 80;
    gap[secondFrameOffset + 4] = 42;
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(gap.data(), gap.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::SequenceGap),
        static_cast<uint8_t>(decoder.error()));

    CsiCaptureFrame frames[] = {makeFrame(100), makeFrame(102, 10, false)};
    CsiCaptureError error = CsiCaptureError::None;
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(frames, 2, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::SequenceGap),
        static_cast<uint8_t>(error));
}

void test_decoder_and_encoder_require_modular_monotonic_process_time() {
    CsiCaptureFrame wrappingFrames[] = {
        makeFrame(100),
        makeFrame(101, 10, false),
    };
    wrappingFrames[0].processNowMs = 0xfffffff0u;
    wrappingFrames[1].processNowMs = 0x00000010u;
    CsiCaptureError error = CsiCaptureError::None;
    TEST_ASSERT_NOT_EQUAL(0, encodedCsiCaptureSize(wrappingFrames, 2, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::None),
        static_cast<uint8_t>(error));
    std::array<uint8_t, kTestBufferBytes> wrappingEncoded{};
    size_t wrappingBytes = 0;
    TEST_ASSERT_TRUE(encodeCsiCapture(
        7,
        wrappingFrames,
        2,
        wrappingEncoded.data(),
        wrappingEncoded.size(),
        wrappingBytes,
        error));
    CsiCaptureDecoder wrappingDecoder;
    TEST_ASSERT_TRUE(wrappingDecoder.open(wrappingEncoded.data(), wrappingBytes));

    CsiCaptureFrame backwardsFrames[] = {
        makeFrame(200),
        makeFrame(201, 10, false),
    };
    backwardsFrames[0].processNowMs = 1000;
    backwardsFrames[1].processNowMs = 900;
    TEST_ASSERT_EQUAL_UINT32(
        0,
        encodedCsiCaptureSize(backwardsFrames, 2, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::NonMonotonicProcessTime),
        static_cast<uint8_t>(error));

    auto malformed = copyTinyFixture<FIXTURES::TINY_CSI_CAPTURE_V1_SIZE>();
    constexpr size_t secondFrameOffset = 32 + 80;
    const uint32_t backwardsTime = 900;
    malformed[secondFrameOffset + 8] = static_cast<uint8_t>(backwardsTime);
    malformed[secondFrameOffset + 9] = static_cast<uint8_t>(backwardsTime >> 8u);
    malformed[secondFrameOffset + 10] = static_cast<uint8_t>(backwardsTime >> 16u);
    malformed[secondFrameOffset + 11] = static_cast<uint8_t>(backwardsTime >> 24u);
    CsiCaptureDecoder decoder;
    TEST_ASSERT_FALSE(decoder.open(malformed.data(), malformed.size()));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::NonMonotonicProcessTime),
        static_cast<uint8_t>(decoder.error()));
}

void test_codec_rejects_inconsistent_truncation_and_undersized_output() {
    CsiCaptureFrame frame = makeFrame(1);
    frame.originalLength = 20;
    CsiCaptureError error = CsiCaptureError::None;
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(&frame, 1, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::InvalidTruncationFlag),
        static_cast<uint8_t>(error));

    frame.originalLength = frame.storedLength;
    std::array<uint8_t, 16> tooSmall{};
    size_t bytesWritten = 99;
    TEST_ASSERT_FALSE(encodeCsiCapture(
        1, &frame, 1, tooSmall.data(), tooSmall.size(), bytesWritten, error));
    TEST_ASSERT_EQUAL_UINT32(0, bytesWritten);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::OutputTooSmall),
        static_cast<uint8_t>(error));

    frame.originalLength = 20;
    frame.flags |= Truncated;
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(&frame, 1, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::TruncatedFrame),
        static_cast<uint8_t>(error));

    frame.originalLength = 17;
    frame.storedLength = 16;
    frame.flags |= Truncated;
    TEST_ASSERT_EQUAL_UINT32(0, encodedCsiCaptureSize(&frame, 1, error));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CsiCaptureError::OddOriginalLength),
        static_cast<uint8_t>(error));
}

void test_decoder_cursor_is_bounded_even_if_caller_tampers_with_it() {
    CsiCaptureDecoder decoder;
    TEST_ASSERT_TRUE(decoder.open(
        FIXTURES::TINY_CSI_CAPTURE_V1,
        FIXTURES::TINY_CSI_CAPTURE_V1_SIZE));
    CsiCaptureFrameCursor cursor = decoder.beginFrames();
    cursor.offset = static_cast<size_t>(-1);
    CsiCaptureFrame frame;
    TEST_ASSERT_FALSE(decoder.hasNextFrame(cursor));
    TEST_ASSERT_FALSE(decoder.nextFrame(cursor, frame));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_codec_round_trip_preserves_lossless_frame_metadata_and_iq);
    RUN_TEST(test_hand_authored_embedded_fixture_decodes_with_canonical_offsets);
    RUN_TEST(test_replay_invokes_exact_production_detector_and_ignores_observed_output);
    RUN_TEST(test_decoder_rejects_unsupported_version_and_wrong_endian_marker);
    RUN_TEST(test_codec_rejects_empty_capture);
    RUN_TEST(test_decoder_rejects_truncated_header_payload_and_trailing_bytes);
    RUN_TEST(test_decoder_rejects_oversized_u64_section_without_integer_wrap);
    RUN_TEST(test_decoder_rejects_malformed_record_lengths_flags_and_reserved_bytes);
    RUN_TEST(test_decoder_and_encoder_require_exactly_one_initial_replay_origin);
    RUN_TEST(test_decoder_and_encoder_reject_sequence_gaps);
    RUN_TEST(test_decoder_and_encoder_require_modular_monotonic_process_time);
    RUN_TEST(test_codec_rejects_inconsistent_truncation_and_undersized_output);
    RUN_TEST(test_decoder_cursor_is_bounded_even_if_caller_tampers_with_it);
    RUN_TEST(test_scenario_parser_requires_reviewed_real_data_contract);
    RUN_TEST(test_replay_metrics_measure_errors_latency_hold_and_clear);
    RUN_TEST(test_replay_metrics_mark_frame_staleness_unavailable);
    RUN_TEST(test_fixture_runner_sha256_matches_known_vector);
    RUN_TEST(test_real_csi_fixture_corpus_when_explicitly_enabled);
    return UNITY_END();
}
