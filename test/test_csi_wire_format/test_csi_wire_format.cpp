#include <unity.h>

#include "../../src/api/wifisensing/CsiWireFormat.h"
#include "../../src/api/wifisensing/CsiWireFormat.cpp"

using WIFISENSING::CSI::CsiPacket;

void setUp(void) {}
void tearDown(void) {}

static CsiPacket makePacket(uint32_t timestamp,
                            int8_t rssi,
                            float gain,
                            const int8_t* data,
                            size_t len) {
    CsiPacket packet = {};
    packet.rx_ctrl.timestamp = timestamp;
    packet.rx_ctrl.rssi = rssi;
    packet.len = len;
    packet.compensate_gain = gain;
    packet.motionScore = 2.5f;
    packet.isMotionDetected = true;
    memcpy(packet.buf, data, len);
    return packet;
}

void test_write_record_encodes_single_packet() {
    const int8_t iq[] = {3, 4, 5, 12};
    CsiPacket packet = makePacket(0x01020304, -42, 1.5f, iq, sizeof(iq));

    uint8_t buffer[API::CSI_WIRE::RECORD_MAX_BYTES] = {};
    const size_t written = API::CSI_WIRE::writeRecord(buffer, sizeof(buffer), packet);

    TEST_ASSERT_EQUAL_UINT32(API::CSI_WIRE::RECORD_HEADER_BYTES + sizeof(iq), written);
    TEST_ASSERT_EQUAL_UINT8(0x04, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0x03, buffer[1]);
    TEST_ASSERT_EQUAL_UINT8(0x02, buffer[2]);
    TEST_ASSERT_EQUAL_UINT8(0x01, buffer[3]);
    TEST_ASSERT_EQUAL_INT8(-42, static_cast<int8_t>(buffer[4]));
    TEST_ASSERT_EQUAL_UINT8(sizeof(iq), buffer[5]);
    TEST_ASSERT_EQUAL_UINT8(0, buffer[6]);
    TEST_ASSERT_EQUAL_UINT8(15, buffer[7]);
    TEST_ASSERT_EQUAL_UINT8(1, buffer[12]);
    TEST_ASSERT_EQUAL_INT8(3, static_cast<int8_t>(buffer[13]));
    TEST_ASSERT_EQUAL_INT8(12, static_cast<int8_t>(buffer[16]));
}

void test_write_batch_concatenates_packet_records() {
    const int8_t firstIq[] = {1, 2};
    const int8_t secondIq[] = {3, 4, 5, 6};
    CsiPacket packets[] = {
        makePacket(1, -40, 1.0f, firstIq, sizeof(firstIq)),
        makePacket(2, -50, 2.0f, secondIq, sizeof(secondIq)),
    };

    uint8_t buffer[API::CSI_WIRE::BATCH_MAX_BYTES] = {};
    const size_t written = API::CSI_WIRE::writeBatch(buffer, sizeof(buffer), packets, 2);
    const size_t firstRecordLen = API::CSI_WIRE::RECORD_HEADER_BYTES + sizeof(firstIq);

    TEST_ASSERT_EQUAL_UINT32(
        (API::CSI_WIRE::RECORD_HEADER_BYTES * 2) + sizeof(firstIq) + sizeof(secondIq),
        written);
    TEST_ASSERT_EQUAL_UINT8(1, buffer[0]);
    TEST_ASSERT_EQUAL_INT8(-40, static_cast<int8_t>(buffer[4]));
    TEST_ASSERT_EQUAL_UINT8(2, buffer[firstRecordLen]);
    TEST_ASSERT_EQUAL_INT8(-50, static_cast<int8_t>(buffer[firstRecordLen + 4]));
}

void test_write_batch_rejects_undersized_buffer() {
    const int8_t iq[] = {1, 2};
    CsiPacket packet = makePacket(1, -40, 1.0f, iq, sizeof(iq));

    uint8_t buffer[API::CSI_WIRE::RECORD_HEADER_BYTES] = {};
    TEST_ASSERT_EQUAL_UINT32(0, API::CSI_WIRE::writeBatch(buffer, sizeof(buffer), &packet, 1));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_write_record_encodes_single_packet);
    RUN_TEST(test_write_batch_concatenates_packet_records);
    RUN_TEST(test_write_batch_rejects_undersized_buffer);
    return UNITY_END();
}
