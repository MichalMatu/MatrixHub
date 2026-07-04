#include <unity.h>

#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

#include <LittleFS.h>

#include "../../src/system/reset/FactoryReset.cpp"

FS LittleFS;

namespace TEST_FACTORY_RESET {

inline int shutdownCalls = 0;
inline SYSTEM::ShutdownReason shutdownReason = SYSTEM::ShutdownReason::UNKNOWN;

template <typename T>
unsigned char* rawStorage() {
    alignas(T) static unsigned char storage[sizeof(T)];
    return storage;
}

template <typename T>
T& rawObject() {
    return *reinterpret_cast<T*>(rawStorage<T>());
}

template <typename T>
void clearRawObject() {
    std::memset(rawStorage<T>(), 0, sizeof(T));
}

void reset() {
    shutdownCalls = 0;
    shutdownReason = SYSTEM::ShutdownReason::UNKNOWN;

    TEST_STUBS::PREFERENCES::reset();
    TEST_STUBS::FILESYSTEM::reset();
    TEST_STUBS::ESP::reset();
    TEST_STUBS::FREERTOS::resetTaskCreateStub();
    TEST_STUBS::FREERTOS::tickCount = 0;
    TEST_STUBS::ARDUINO::millisValue = 0;

    clearRawObject<ServiceRegistry>();
}

}  // namespace TEST_FACTORY_RESET

namespace LOG {

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::logSection(const char* title) {
    (void)title;
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

}  // namespace LOG

namespace SYSTEM {

void ShutdownSequence::execute(ServiceRegistry& registry, ShutdownReason explicitReason) {
    (void)registry;
    TEST_FACTORY_RESET::shutdownCalls++;
    TEST_FACTORY_RESET::shutdownReason = explicitReason;
}

}  // namespace SYSTEM

void setUp(void) {
    TEST_FACTORY_RESET::reset();
}

void tearDown(void) {}

void test_performFactoryReset_clears_prefs_formats_fs_and_restarts() {
    auto& registry = TEST_FACTORY_RESET::rawObject<ServiceRegistry>();
    TEST_STUBS::FILESYSTEM::beginResult = true;
    TEST_STUBS::FILESYSTEM::formatResult = true;

    SYSTEM::performFactoryReset(registry);

    TEST_ASSERT_EQUAL_INT(1, TEST_FACTORY_RESET::shutdownCalls);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(SYSTEM::ShutdownReason::FACTORY_RESET),
                            static_cast<uint8_t>(TEST_FACTORY_RESET::shutdownReason));

    const std::vector<std::string> expectedNamespaces = {
        "log_cfg",
        "power_cfg",
        "sensor_cal",
        "scd4x_comp",
    };
    TEST_ASSERT_EQUAL(expectedNamespaces.size(), TEST_STUBS::PREFERENCES::clearedNamespaces.size());
    for (size_t i = 0; i < expectedNamespaces.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expectedNamespaces[i].c_str(),
                                 TEST_STUBS::PREFERENCES::clearedNamespaces[i].c_str());
    }

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::FILESYSTEM::endCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::FILESYSTEM::beginCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::FILESYSTEM::formatCalls);
    TEST_ASSERT_EQUAL_UINT32(FACTORY::PRE_RESTART_DELAY_MS, TEST_STUBS::FREERTOS::tickCount);
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::ESP::restartCalls);
}

void test_performFactoryReset_formats_and_restarts_even_when_mount_fails() {
    auto& registry = TEST_FACTORY_RESET::rawObject<ServiceRegistry>();
    TEST_STUBS::FILESYSTEM::beginResult = false;

    SYSTEM::performFactoryReset(registry);

    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::FILESYSTEM::endCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::FILESYSTEM::beginCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_STUBS::FILESYSTEM::formatCalls);
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::ESP::restartCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_performFactoryReset_clears_prefs_formats_fs_and_restarts);
    RUN_TEST(test_performFactoryReset_formats_and_restarts_even_when_mount_fails);
    return UNITY_END();
}
