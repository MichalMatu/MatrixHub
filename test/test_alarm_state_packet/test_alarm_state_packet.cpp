#include <unity.h>

#include <cstring>

#include "../../src/api/system/broadcasters/AlarmStatePacket.h"

namespace {

ALARMS::AlarmStateChange makeChange() {
    ALARMS::AlarmStateChange change{};
    std::strncpy(change.id, "alarm-7", sizeof(change.id) - 1);
    change.triggered = true;
    change.currentValue = 42.5f;
    change.severity = ALARMS::AlarmSeverity::Critical;
    change.transitionSeq = 0x78563412U;
    change.deviceMillis = 0xEFCDAB90U;
    change.bootId = 0x8877665544332211ULL;
    return change;
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_alarm_packet_preserves_legacy_prefix_and_appends_metadata() {
    const ALARMS::AlarmStateChange change = makeChange();
    uint8_t packet[API::kAlarmStatePacketSize]{};

    const size_t written =
        API::encodeAlarmStatePacket(change, packet, sizeof(packet));

    TEST_ASSERT_EQUAL_UINT32(API::kAlarmStatePacketSize, written);
    TEST_ASSERT_EQUAL_HEX8(API::kAlarmStateMagic, packet[0]);
    TEST_ASSERT_EQUAL_MEMORY(change.id, &packet[1], ALARMS::kMaxIdLen);
    TEST_ASSERT_EQUAL_UINT8(1, packet[33]);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(ALARMS::AlarmSeverity::Critical), packet[34]);

    float decodedValue = 0.0f;
    memcpy(&decodedValue, &packet[35], sizeof(decodedValue));
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 42.5f, decodedValue);

    // Append-only little-endian suffix. Bytes 0..38 remain the legacy packet.
    const uint8_t expectedSuffix[] = {
        0x12, 0x34, 0x56, 0x78,
        0x90, 0xAB, 0xCD, 0xEF,
        0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88,
    };
    TEST_ASSERT_EQUAL_MEMORY(
        expectedSuffix,
        &packet[API::kAlarmStateLegacyPacketSize],
        sizeof(expectedSuffix));
}

void test_alarm_packet_rejects_missing_or_short_output() {
    const ALARMS::AlarmStateChange change = makeChange();
    uint8_t shortPacket[API::kAlarmStatePacketSize - 1]{};

    TEST_ASSERT_EQUAL_UINT32(
        0,
        API::encodeAlarmStatePacket(change, nullptr, API::kAlarmStatePacketSize));
    TEST_ASSERT_EQUAL_UINT32(
        0,
        API::encodeAlarmStatePacket(change, shortPacket, sizeof(shortPacket)));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_alarm_packet_preserves_legacy_prefix_and_appends_metadata);
    RUN_TEST(test_alarm_packet_rejects_missing_or_short_output);
    return UNITY_END();
}
