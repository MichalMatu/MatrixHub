#include <unity.h>

#include <cstdarg>
#include <WiFi.h>

#include "../../src/system/logging/Logging.h"

WiFiClass WiFi;

namespace LOG {

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

}  // namespace LOG

#include "../../src/wifisensing/sampling/RssiSampler.cpp"

void setUp(void) {
    TEST_STUBS::WIFI::reset();
}

void tearDown(void) {}

void test_init_never_mutates_storage_without_buffer_lock() {
    WIFISENSING::RssiSampler sampler;
    SemaphoreHandle_t mutex = sampler.getMutex();
    TEST_ASSERT_NOT_NULL(mutex);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, portMAX_DELAY));

    TEST_ASSERT_FALSE(sampler.init(0));
    TEST_ASSERT_FALSE(sampler.isInitialized());

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(mutex));
    TEST_ASSERT_TRUE(sampler.init(0));
    TEST_ASSERT_TRUE(sampler.isInitialized());
}

void test_deinit_timeout_retains_live_storage_until_retry() {
    WIFISENSING::RssiSampler sampler;
    TEST_ASSERT_TRUE(sampler.init(0));
    (void)sampler.takeSample();
    TEST_ASSERT_EQUAL_UINT16(1, sampler.getCount());

    SemaphoreHandle_t mutex = sampler.getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, portMAX_DELAY));

    TEST_ASSERT_FALSE(sampler.deinit(0));
    TEST_ASSERT_TRUE(sampler.isInitialized());

    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(mutex));
    TEST_ASSERT_EQUAL_UINT16(1, sampler.getCount());
    TEST_ASSERT_TRUE(sampler.deinit(0));
    TEST_ASSERT_FALSE(sampler.isInitialized());

    WIFISENSING::RssiSample samples[2]{};
    TEST_ASSERT_EQUAL_UINT16(0, sampler.getSamples(samples, 2));
}

void test_reinit_safely_reuses_and_resets_retained_storage() {
    WIFISENSING::RssiSampler sampler;
    TEST_ASSERT_TRUE(sampler.init(0));
    (void)sampler.takeSample();
    TEST_ASSERT_EQUAL_UINT16(1, sampler.getCount());

    SemaphoreHandle_t mutex = sampler.getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(mutex, portMAX_DELAY));
    TEST_ASSERT_FALSE(sampler.deinit(0));
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(mutex));

    TEST_ASSERT_TRUE(sampler.init(0));
    TEST_ASSERT_TRUE(sampler.isInitialized());
    TEST_ASSERT_EQUAL_UINT16(0, sampler.getCount());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_init_never_mutates_storage_without_buffer_lock);
    RUN_TEST(test_deinit_timeout_retains_live_storage_until_retry);
    RUN_TEST(test_reinit_safely_reuses_and_resets_retained_storage);
    return UNITY_END();
}
