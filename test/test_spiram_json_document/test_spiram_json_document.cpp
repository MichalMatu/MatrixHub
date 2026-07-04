#include <unity.h>
#include <ArduinoJson.h>

#include <string>

#include "../../src/system/memory/PsramAllocator.h"

void setUp(void) {}
void tearDown(void) {}

void test_bounded_document_accepts_json_within_limit() {
    constexpr size_t kRequestedLimit = 2048;
    SYSTEM::SpiRamJsonDocument doc(kRequestedLimit);

    DeserializationError err = deserializeJson(doc, "{\"ok\":true,\"items\":[1,2,3]}");

    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_FALSE(doc.overflowed());
    TEST_ASSERT_TRUE(doc["ok"].as<bool>());
    TEST_ASSERT_EQUAL(3, doc["items"].as<JsonArray>().size());
    TEST_ASSERT_LESS_OR_EQUAL(SYSTEM::SpiRamJsonDocument::effectiveLimitFor(kRequestedLimit),
                              doc.peakAllocatedBytes());
}

void test_bounded_document_rejects_json_beyond_limit() {
    constexpr size_t kRequestedLimit = 128;
    SYSTEM::SpiRamJsonDocument doc(kRequestedLimit);
    std::string json = "[";
    for (size_t i = 0; i < 300; ++i) {
        if (i > 0) {
            json += ",";
        }
        json += "1";
    }
    json += "]";

    DeserializationError err = deserializeJson(doc, json);

    TEST_ASSERT_TRUE(err == DeserializationError::NoMemory || doc.overflowed());
    TEST_ASSERT_TRUE(doc.overflowed());
    TEST_ASSERT_LESS_OR_EQUAL(SYSTEM::SpiRamJsonDocument::effectiveLimitFor(kRequestedLimit),
                              doc.peakAllocatedBytes());
}

void test_small_requested_limit_uses_arduinojson_minimum_pool() {
    SYSTEM::SpiRamJsonDocument doc(32);

    TEST_ASSERT_EQUAL(32, doc.requestedCapacityLimit());
    TEST_ASSERT_EQUAL(SYSTEM::SpiRamJsonDocument::effectiveLimitFor(32), doc.capacityLimit());
}

void test_zero_capacity_document_keeps_unbounded_compatibility() {
    SYSTEM::SpiRamJsonDocument doc;

    DeserializationError err = deserializeJson(
        doc,
        "{\"name\":\"abcdefghijklmnopqrstuvwxyz\",\"items\":[1,2,3,4,5,6,7,8]}");

    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_FALSE(doc.overflowed());
    TEST_ASSERT_EQUAL(0, doc.capacityLimit());
    TEST_ASSERT_TRUE(doc.peakAllocatedBytes() >= SYSTEM::SpiRamJsonDocument::minimumCapacity());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_bounded_document_accepts_json_within_limit);
    RUN_TEST(test_bounded_document_rejects_json_beyond_limit);
    RUN_TEST(test_small_requested_limit_uses_arduinojson_minimum_pool);
    RUN_TEST(test_zero_capacity_document_keeps_unbounded_compatibility);
    return UNITY_END();
}
