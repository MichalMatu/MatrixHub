#include <unity.h>

#include <cstring>

#include "../../src/api/wifisensing/CsiCaptureWireFormat.h"
#include "../../src/api/wifisensing/CsiCaptureWireFormat.cpp"

using WIFISENSING::CSI::CsiPacket;

namespace {

uint16_t readU16(const uint8_t* value) {
    return static_cast<uint16_t>(value[0]) |
           (static_cast<uint16_t>(value[1]) << 8u);
}

uint32_t readU32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8u) |
           (static_cast<uint32_t>(value[2]) << 16u) |
           (static_cast<uint32_t>(value[3]) << 24u);
}

uint32_t floatBits(float value) {
    uint32_t bits = 0;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

CsiPacket makePacket() {
    CsiPacket packet{};
    packet.acceptedSequence = 0xf1020304u;
    packet.processTimestampMs = 0x11223344u;
    packet.rx_ctrl.timestamp = 0xa1b2c3d4u;
    packet.compensate_gain = 1.234567f;
    packet.motionScore = 91.125f;
    packet.originalLen = 6;
    packet.len = 6;
    packet.rxSequence = 0x4567;
    packet.rx_ctrl.sig_len = 0x345;
    const uint8_t source[] = {0x02, 0x10, 0x20, 0x30, 0x40, 0x50};
    const uint8_t destination[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    memcpy(packet.mac, source, sizeof(source));
    memcpy(packet.dmac, destination, sizeof(destination));
    packet.rx_ctrl.rssi = -67;
    packet.rx_ctrl.noise_floor = -96;
    packet.rx_ctrl.rate = 7;
    packet.rx_ctrl.sig_mode = 1;
    packet.rx_ctrl.mcs = 5;
    packet.rx_ctrl.cwb = 1;
    packet.rx_ctrl.smoothing = 1;
    packet.rx_ctrl.not_sounding = 1;
    packet.rx_ctrl.aggregation = 1;
    packet.rx_ctrl.stbc = 2;
    packet.rx_ctrl.fec_coding = 1;
    packet.rx_ctrl.sgi = 1;
    packet.rx_ctrl.ampdu_cnt = 3;
    packet.rx_ctrl.channel = 6;
    packet.rx_ctrl.secondary_channel = 1;
    packet.rx_ctrl.ant = 1;
    packet.rx_ctrl.rx_state = 4;
    packet.firstWordInvalid = true;
    packet.isMotionDetected = true;
    const int8_t iq[] = {-1, 2, -3, 4, -5, 6};
    memcpy(packet.buf, iq, sizeof(iq));
    return packet;
}

} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_record_encodes_canonical_64_byte_header() {
    const CsiPacket packet = makePacket();
    uint8_t output[API::CSI_CAPTURE_WIRE::RECORD_MAX_BYTES] = {};

    const size_t written =
        API::CSI_CAPTURE_WIRE::writeRecord(output, sizeof(output), packet, true);

    TEST_ASSERT_EQUAL_UINT32(70, written);
    TEST_ASSERT_EQUAL_UINT16(70, readU16(output));
    TEST_ASSERT_EQUAL_UINT16(64, readU16(output + 2));
    TEST_ASSERT_EQUAL_HEX32(packet.acceptedSequence, readU32(output + 4));
    TEST_ASSERT_EQUAL_HEX32(packet.processTimestampMs, readU32(output + 8));
    TEST_ASSERT_EQUAL_HEX32(packet.rx_ctrl.timestamp, readU32(output + 12));
    TEST_ASSERT_EQUAL_HEX32(floatBits(packet.compensate_gain), readU32(output + 16));
    TEST_ASSERT_EQUAL_HEX32(floatBits(packet.motionScore), readU32(output + 20));
    TEST_ASSERT_EQUAL_UINT16(6, readU16(output + 24));
    TEST_ASSERT_EQUAL_UINT16(6, readU16(output + 26));
    TEST_ASSERT_EQUAL_UINT16(packet.rxSequence, readU16(output + 28));
    TEST_ASSERT_EQUAL_UINT16(packet.rx_ctrl.sig_len, readU16(output + 30));
    TEST_ASSERT_EQUAL_MEMORY(packet.mac, output + 32, 6);
    TEST_ASSERT_EQUAL_MEMORY(packet.dmac, output + 38, 6);
    TEST_ASSERT_EQUAL_INT8(-67, static_cast<int8_t>(output[44]));
    TEST_ASSERT_EQUAL_INT8(-96, static_cast<int8_t>(output[45]));
    TEST_ASSERT_EQUAL_UINT8(7, output[46]);
    TEST_ASSERT_EQUAL_UINT8(4, output[60]);
    TEST_ASSERT_EQUAL_HEX8(
        API::CSI_CAPTURE_WIRE::FIRST_WORD_INVALID |
            API::CSI_CAPTURE_WIRE::OBSERVED_MOTION |
            API::CSI_CAPTURE_WIRE::REPLAY_ORIGIN,
        output[61]);
    TEST_ASSERT_EQUAL_UINT16(0, readU16(output + 62));
    TEST_ASSERT_EQUAL_MEMORY(packet.buf, output + 64, packet.len);
}

void test_data_batch_has_versioned_header_and_session() {
    const CsiPacket packet = makePacket();
    uint8_t output[API::CSI_CAPTURE_WIRE::BATCH_MAX_BYTES] = {};

    const size_t written = API::CSI_CAPTURE_WIRE::writeBatch(
        output, sizeof(output), 0x55667788u, &packet, 1, true);

    TEST_ASSERT_EQUAL_UINT32(API::CSI_CAPTURE_WIRE::BATCH_HEADER_BYTES + 70, written);
    TEST_ASSERT_EQUAL_MEMORY("MHCB", output, 4);
    TEST_ASSERT_EQUAL_UINT8(1, output[4]);
    TEST_ASSERT_EQUAL_UINT8(0, output[5]);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(API::CSI_CAPTURE_WIRE::MessageType::Data), output[6]);
    TEST_ASSERT_EQUAL_UINT8(16, output[7]);
    TEST_ASSERT_EQUAL_UINT16(64, readU16(output + 8));
    TEST_ASSERT_EQUAL_UINT16(1, readU16(output + 10));
    TEST_ASSERT_EQUAL_HEX32(0x55667788u, readU32(output + 12));
    TEST_ASSERT_BITS_HIGH(
        API::CSI_CAPTURE_WIRE::REPLAY_ORIGIN,
        output[API::CSI_CAPTURE_WIRE::BATCH_HEADER_BYTES + 61]);
}

void test_control_messages_are_fixed_size_and_canonical() {
    API::CSI_CAPTURE_WIRE::HelloPayload hello;
    hello.startedAtMs = 100;
    hello.rxFramesStart = 10;
    hello.rxAcceptedStart = 5;
    hello.queuedPacketsStart = 5;
    hello.sourceQueueDropsStart = 0;
    hello.rxThrottleIntervalUs = 60000;
    hello.motionControlEpoch = 0x12345678u;
    uint8_t helloBytes[API::CSI_CAPTURE_WIRE::HELLO_MESSAGE_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(
        sizeof(helloBytes),
        API::CSI_CAPTURE_WIRE::writeHello(
            helloBytes, sizeof(helloBytes), 7, hello));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(API::CSI_CAPTURE_WIRE::MessageType::Hello), helloBytes[6]);
    TEST_ASSERT_EQUAL_UINT16(64, readU16(helloBytes + 8));
    TEST_ASSERT_EQUAL_UINT32(60000, readU32(helloBytes + 36));
    TEST_ASSERT_EQUAL_UINT16(512, readU16(helloBytes + 40));
    TEST_ASSERT_EQUAL_UINT16(10, readU16(helloBytes + 42));
    TEST_ASSERT_EQUAL_HEX16(API::CSI_CAPTURE_WIRE::CAPABILITY_FLAGS, readU16(helloBytes + 46));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, readU32(helloBytes + 52));

    API::CSI_CAPTURE_WIRE::EndPayload end;
    end.stoppedAtMs = 200;
    end.firstAcceptedSequence = 40;
    end.lastAcceptedSequence = 41;
    end.recordsOffered = 2;
    end.recordsEnqueued = 2;
    end.dataBatchesOffered = 1;
    end.dataBatchesEnqueued = 1;
    end.sessionErrorFlags = API::CSI_CAPTURE_WIRE::SOURCE_SEQUENCE_INCOMPLETE;
    uint8_t endBytes[API::CSI_CAPTURE_WIRE::END_MESSAGE_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(
        sizeof(endBytes),
        API::CSI_CAPTURE_WIRE::writeEnd(endBytes, sizeof(endBytes), 7, end));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(API::CSI_CAPTURE_WIRE::MessageType::End), endBytes[6]);
    TEST_ASSERT_EQUAL_UINT32(40, readU32(endBytes + 24));
    TEST_ASSERT_EQUAL_UINT32(2, readU32(endBytes + 32));
    TEST_ASSERT_EQUAL_HEX32(
        API::CSI_CAPTURE_WIRE::SOURCE_SEQUENCE_INCOMPLETE,
        readU32(endBytes + 76));
}

void test_truncation_is_explicit_and_capacity_is_fail_closed() {
    CsiPacket packet = makePacket();
    packet.originalLen = 8;
    uint8_t output[API::CSI_CAPTURE_WIRE::RECORD_MAX_BYTES] = {};
    const size_t written =
        API::CSI_CAPTURE_WIRE::writeRecord(output, sizeof(output), packet);
    TEST_ASSERT_EQUAL_UINT32(70, written);
    TEST_ASSERT_BITS_HIGH(API::CSI_CAPTURE_WIRE::INPUT_TRUNCATED, output[61]);

    TEST_ASSERT_EQUAL_UINT32(
        0,
        API::CSI_CAPTURE_WIRE::writeRecord(
            output, API::CSI_CAPTURE_WIRE::RECORD_HEADER_BYTES, packet));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_record_encodes_canonical_64_byte_header);
    RUN_TEST(test_data_batch_has_versioned_header_and_session);
    RUN_TEST(test_control_messages_are_fixed_size_and_canonical);
    RUN_TEST(test_truncation_is_explicit_and_capacity_is_fail_closed);
    return UNITY_END();
}
