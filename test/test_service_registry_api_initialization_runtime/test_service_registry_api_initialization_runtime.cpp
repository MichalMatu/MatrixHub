#include <unity.h>

#include <string>
#include <vector>

#include <core/ESP32SvelteKit.h>

#include "../../src/system/services/ServiceRegistryOwnedApiRuntime.cpp"

namespace TEST_SERVICE_REGISTRY_API_RUNTIME {

struct CallLog {
    std::vector<std::string> entries;

    void clear() {
        entries.clear();
    }

    void push(const char* entry) {
        entries.emplace_back(entry);
    }
};

inline CallLog calls;

inline int macroCtorCalls = 0;
inline int macroBeginCalls = 0;
inline PsychicHttpServer* lastMacroServer = nullptr;
inline SecurityManager* lastMacroSecurityManager = nullptr;
inline POWER::PowerManager* lastMacroPowerManager = nullptr;
inline MACROS::MacroService* lastMacroService = nullptr;
inline MACROS::MacroSettingsService* lastMacroSettings = nullptr;

inline int airMouseCtorCalls = 0;
inline int airMouseBeginCalls = 0;
inline PsychicHttpServer* lastAirMouseServer = nullptr;
inline SecurityManager* lastAirMouseSecurityManager = nullptr;
inline POWER::PowerManager* lastAirMousePowerManager = nullptr;
inline AIRMOUSE::AirMouseService* lastAirMouseService = nullptr;
inline MACROS::MacroService* lastAirMouseMacroService = nullptr;
inline AIRMOUSE::AirMouseSettingsService* lastAirMouseSettings = nullptr;
inline SemaphoreHandle_t lastAirMouseFsMutexAtBegin = nullptr;

void reset() {
    calls.clear();

    macroCtorCalls = 0;
    macroBeginCalls = 0;
    lastMacroServer = nullptr;
    lastMacroSecurityManager = nullptr;
    lastMacroPowerManager = nullptr;
    lastMacroService = nullptr;
    lastMacroSettings = nullptr;

    airMouseCtorCalls = 0;
    airMouseBeginCalls = 0;
    lastAirMouseServer = nullptr;
    lastAirMouseSecurityManager = nullptr;
    lastAirMousePowerManager = nullptr;
    lastAirMouseService = nullptr;
    lastAirMouseMacroService = nullptr;
    lastAirMouseSettings = nullptr;
    lastAirMouseFsMutexAtBegin = nullptr;
}

}  // namespace TEST_SERVICE_REGISTRY_API_RUNTIME

namespace MACROS {

MacroApiService::MacroApiService(PsychicHttpServer* server,
                                 SecurityManager* securityManager,
                                 POWER::PowerManager* powerManager,
                                 MacroService* service,
                                 MacroSettingsService* settings)
    : API::BaseApiService(server, securityManager, powerManager, "test/macro"),
      _service(service),
      _settings(settings) {
    TEST_SERVICE_REGISTRY_API_RUNTIME::calls.push("macroCtor");
    TEST_SERVICE_REGISTRY_API_RUNTIME::macroCtorCalls++;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroServer = server;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroSecurityManager = securityManager;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroPowerManager = powerManager;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroService = service;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroSettings = settings;
}

void MacroApiService::begin() {
    TEST_SERVICE_REGISTRY_API_RUNTIME::calls.push("macroBegin");
    TEST_SERVICE_REGISTRY_API_RUNTIME::macroBeginCalls++;
}

}  // namespace MACROS

namespace API {

AirMouseApiService::AirMouseApiService(PsychicHttpServer* server,
                                       SecurityManager* securityManager,
                                       POWER::PowerManager* powerManager,
                                       AIRMOUSE::AirMouseService* service,
                                       MACROS::MacroService* macroService,
                                       AIRMOUSE::AirMouseSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "test/airmouse"),
      _service(service),
      _macroService(macroService),
      _settings(settings) {
    TEST_SERVICE_REGISTRY_API_RUNTIME::calls.push("airMouseCtor");
    TEST_SERVICE_REGISTRY_API_RUNTIME::airMouseCtorCalls++;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseServer = server;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseSecurityManager = securityManager;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMousePowerManager = powerManager;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseService = service;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseMacroService = macroService;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseSettings = settings;
}

void AirMouseApiService::begin() {
    TEST_SERVICE_REGISTRY_API_RUNTIME::calls.push("airMouseBegin");
    TEST_SERVICE_REGISTRY_API_RUNTIME::airMouseBeginCalls++;
    TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseFsMutexAtBegin = _fsMutex;
}

void AirMouseApiService::cleanupClient(int fd) {
    (void)fd;
}

}  // namespace API

void setUp(void) {
    TEST_SERVICE_REGISTRY_API_RUNTIME::reset();
}

void tearDown(void) {}

void test_initializeMacroApiService_constructs_and_begins_with_forwarded_dependencies() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    SYSTEM::PsramStaticService<MACROS::MacroApiService> macroApi{};

    auto* powerManager = reinterpret_cast<POWER::PowerManager*>(0x2000);
    auto* macroService = reinterpret_cast<MACROS::MacroService*>(0x2001);
    auto* macroSettings = reinterpret_cast<MACROS::MacroSettingsService*>(0x2002);

    SERVICE_REGISTRY_INIT_RUNTIME::initializeMacroApiService(macroApi,
                                                             &server,
                                                             framework.getSecurityManager(),
                                                             powerManager,
                                                             macroService,
                                                             macroSettings);

    TEST_ASSERT_NOT_NULL(macroApi.get());
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_API_RUNTIME::macroCtorCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_API_RUNTIME::macroBeginCalls);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroServer);
    TEST_ASSERT_EQUAL_PTR(framework.getSecurityManager(),
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroSecurityManager);
    TEST_ASSERT_EQUAL_PTR(powerManager,
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroPowerManager);
    TEST_ASSERT_EQUAL_PTR(macroService, TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroService);
    TEST_ASSERT_EQUAL_PTR(macroSettings, TEST_SERVICE_REGISTRY_API_RUNTIME::lastMacroSettings);

    const std::vector<std::string> expected = {
        "macroCtor",
        "macroBegin",
    };
    TEST_ASSERT_EQUAL(expected.size(), TEST_SERVICE_REGISTRY_API_RUNTIME::calls.entries.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i].c_str(),
                                 TEST_SERVICE_REGISTRY_API_RUNTIME::calls.entries[i].c_str());
    }

    macroApi.destroy();
}

void test_initializeAirMouseApiService_constructs_sets_fs_mutex_and_begins() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    SYSTEM::PsramStaticService<API::AirMouseApiService> airMouseApi{};

    auto* powerManager = reinterpret_cast<POWER::PowerManager*>(0x3000);
    auto* airMouseService = reinterpret_cast<AIRMOUSE::AirMouseService*>(0x3001);
    auto* macroService = reinterpret_cast<MACROS::MacroService*>(0x3002);
    auto* airMouseSettings = reinterpret_cast<AIRMOUSE::AirMouseSettingsService*>(0x3003);
    const auto fsMutex = reinterpret_cast<SemaphoreHandle_t>(0x3004);

    SERVICE_REGISTRY_INIT_RUNTIME::initializeAirMouseApiService(airMouseApi,
                                                                &server,
                                                                framework.getSecurityManager(),
                                                                powerManager,
                                                                airMouseService,
                                                                macroService,
                                                                airMouseSettings,
                                                                fsMutex);

    TEST_ASSERT_NOT_NULL(airMouseApi.get());
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_API_RUNTIME::airMouseCtorCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_SERVICE_REGISTRY_API_RUNTIME::airMouseBeginCalls);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseServer);
    TEST_ASSERT_EQUAL_PTR(framework.getSecurityManager(),
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseSecurityManager);
    TEST_ASSERT_EQUAL_PTR(powerManager,
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMousePowerManager);
    TEST_ASSERT_EQUAL_PTR(airMouseService,
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseService);
    TEST_ASSERT_EQUAL_PTR(macroService,
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseMacroService);
    TEST_ASSERT_EQUAL_PTR(airMouseSettings,
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseSettings);
    TEST_ASSERT_EQUAL_PTR(fsMutex,
                          TEST_SERVICE_REGISTRY_API_RUNTIME::lastAirMouseFsMutexAtBegin);

    const std::vector<std::string> expected = {
        "airMouseCtor",
        "airMouseBegin",
    };
    TEST_ASSERT_EQUAL(expected.size(), TEST_SERVICE_REGISTRY_API_RUNTIME::calls.entries.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i].c_str(),
                                 TEST_SERVICE_REGISTRY_API_RUNTIME::calls.entries[i].c_str());
    }

    airMouseApi.destroy();
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_initializeMacroApiService_constructs_and_begins_with_forwarded_dependencies);
    RUN_TEST(test_initializeAirMouseApiService_constructs_sets_fs_mutex_and_begins);
    return UNITY_END();
}
