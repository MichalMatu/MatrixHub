#include <unity.h>

#include "../../src/api/system/broadcasters/SensorTelemetryPacket.h"

void test_encodes_last_read_ok_packet_layout() {
    SensorSnapshot snap{};
    snap.co2 = 612;
    snap.temp = 24.1f;
    snap.humid = 45.5f;
    snap.timestamp_ms = 9999;

    uint8_t buffer[API::kSensorTelemetryPacketSize] = {0};
    const size_t written = API::encodeSensorTelemetryPacket(snap, true, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_UINT32(API::kSensorTelemetryPacketSize, written);
    TEST_ASSERT_EQUAL_HEX8(API::kSensorTelemetryMagic, buffer[0]);

    const uint16_t co2 = *reinterpret_cast<uint16_t*>(&buffer[1]);
    const int16_t temp = *reinterpret_cast<int16_t*>(&buffer[3]);
    const uint16_t humid = *reinterpret_cast<uint16_t*>(&buffer[5]);
    const uint32_t ts = *reinterpret_cast<uint32_t*>(&buffer[7]);

    TEST_ASSERT_EQUAL_UINT16(612, co2);
    TEST_ASSERT_EQUAL_INT16(241, temp);
    TEST_ASSERT_EQUAL_UINT16(455, humid);
    TEST_ASSERT_EQUAL_UINT32(9999, ts);
    TEST_ASSERT_EQUAL_HEX8(0x01, buffer[11]);
}

void test_encodes_error_flag_when_last_read_failed() {
    SensorSnapshot snap{};
    snap.co2 = 700;
    snap.temp = 25.5f;
    snap.humid = 50.0f;
    snap.timestamp_ms = 4242;

    uint8_t buffer[API::kSensorTelemetryPacketSize] = {0};
    const size_t written = API::encodeSensorTelemetryPacket(snap, false, buffer, sizeof(buffer));

    TEST_ASSERT_EQUAL_UINT32(API::kSensorTelemetryPacketSize, written);
    TEST_ASSERT_EQUAL_HEX8(0x00, buffer[11]);
}

void test_rejects_too_small_output_buffer() {
    SensorSnapshot snap{};
    uint8_t buffer[API::kSensorTelemetryPacketSize - 1] = {0};
    TEST_ASSERT_EQUAL_UINT32(0, API::encodeSensorTelemetryPacket(snap, true, buffer, sizeof(buffer)));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_encodes_last_read_ok_packet_layout);
    RUN_TEST(test_encodes_error_flag_when_last_read_failed);
    RUN_TEST(test_rejects_too_small_output_buffer);
    return UNITY_END();
}
