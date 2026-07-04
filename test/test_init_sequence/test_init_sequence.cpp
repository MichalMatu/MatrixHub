#include <unity.h>

#include <cstdarg>
#include <string>
#include <vector>

#include <PsychicHttpsServer.h>

#include "../../src/system/button/ButtonHandler.h"
#include "../../src/system/init/core/InitSequence.cpp"

namespace TEST_INIT_SEQUENCE {

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
inline int storageInitCalls = 0;
inline int loggingBeginCalls = 0;
inline int loggingGetCalls = 0;
inline int loggingSetLevelCalls = 0;
inline esp_log_level_t configuredLevel = ESP_LOG_WARN;
inline esp_log_level_t lastSetLevel = ESP_LOG_INFO;
inline int bootTrackerBeginCalls = 0;
inline int powerInitCalls = 0;
inline ServiceRegistry* lastPowerServices = nullptr;
inline int jsonWriterBeginCalls = 0;
inline bool jsonWriterBeginResult = true;
inline int redirectBeginCalls = 0;
inline bool redirectBeginResult = true;
inline const httpd_config_t* lastRedirectBaseConfig = nullptr;
inline int registryBeginCalls = 0;
inline PsychicHttpServer* lastBeginServer = nullptr;
inline ESP32SvelteKit* lastBeginFramework = nullptr;
inline ServiceRegistry* lastBeginServices = nullptr;
inline SemaphoreHandle_t lastNetworkMutex = nullptr;
inline SemaphoreHandle_t lastNotifMutex = nullptr;
inline int runtimeTasksInitCalls = 0;
inline ServiceRegistry* lastRuntimeTasksServices = nullptr;
inline int monitoringInitCalls = 0;
inline ServiceRegistry* lastMonitoringServices = nullptr;
inline ButtonHandler* lastMonitoringButtonHandler = nullptr;
inline int printStatsCalls = 0;

void reset() {
    calls.clear();
    storageInitCalls = 0;
    loggingBeginCalls = 0;
    loggingGetCalls = 0;
    loggingSetLevelCalls = 0;
    configuredLevel = ESP_LOG_WARN;
    lastSetLevel = ESP_LOG_INFO;
    bootTrackerBeginCalls = 0;
    powerInitCalls = 0;
    lastPowerServices = nullptr;
    jsonWriterBeginCalls = 0;
    jsonWriterBeginResult = true;
    redirectBeginCalls = 0;
    redirectBeginResult = true;
    lastRedirectBaseConfig = nullptr;
    registryBeginCalls = 0;
    lastBeginServer = nullptr;
    lastBeginFramework = nullptr;
    lastBeginServices = nullptr;
    lastNetworkMutex = nullptr;
    lastNotifMutex = nullptr;
    runtimeTasksInitCalls = 0;
    lastRuntimeTasksServices = nullptr;
    monitoringInitCalls = 0;
    lastMonitoringServices = nullptr;
    lastMonitoringButtonHandler = nullptr;
    printStatsCalls = 0;
}

template <typename T>
T& rawObject() {
    alignas(T) static unsigned char storage[sizeof(T)];
    return *reinterpret_cast<T*>(storage);
}

}  // namespace TEST_INIT_SEQUENCE

namespace LOG {

void Logging::setLevel(esp_log_level_t level) {
    TEST_INIT_SEQUENCE::calls.push("logging.setLevel");
    TEST_INIT_SEQUENCE::loggingSetLevelCalls++;
    TEST_INIT_SEQUENCE::lastSetLevel = level;
}

const char* Logging::levelToString(esp_log_level_t level) {
    switch (level) {
        case ESP_LOG_WARN:
            return "WARN";
        case ESP_LOG_ERROR:
            return "ERROR";
        case ESP_LOG_INFO:
            return "INFO";
        default:
            return "OTHER";
    }
}

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

namespace LoggingConfig {

void begin() {
    TEST_INIT_SEQUENCE::calls.push("loggingConfig.begin");
    TEST_INIT_SEQUENCE::loggingBeginCalls++;
}

LOG::Settings get() {
    TEST_INIT_SEQUENCE::calls.push("loggingConfig.get");
    TEST_INIT_SEQUENCE::loggingGetCalls++;
    return LOG::Settings{TEST_INIT_SEQUENCE::configuredLevel};
}

bool setLevel(esp_log_level_t level) {
    TEST_INIT_SEQUENCE::lastSetLevel = level;
    return true;
}

}  // namespace LoggingConfig

namespace SYSTEM {

void StorageInitializer::initialize() {
    TEST_INIT_SEQUENCE::calls.push("storage.initialize");
    TEST_INIT_SEQUENCE::storageInitCalls++;
}

void PowerInitializer::initialize(ServiceRegistry& services) {
    TEST_INIT_SEQUENCE::calls.push("power.initialize");
    TEST_INIT_SEQUENCE::powerInitCalls++;
    TEST_INIT_SEQUENCE::lastPowerServices = &services;
}

void RuntimeTasksInitializer::initialize(ServiceRegistry& services) {
    TEST_INIT_SEQUENCE::calls.push("runtimeTasks.initialize");
    TEST_INIT_SEQUENCE::runtimeTasksInitCalls++;
    TEST_INIT_SEQUENCE::lastRuntimeTasksServices = &services;
}

void MonitoringInitializer::initialize(ServiceRegistry& services, ButtonHandler& buttonHandler) {
    TEST_INIT_SEQUENCE::calls.push("monitoring.initialize");
    TEST_INIT_SEQUENCE::monitoringInitCalls++;
    TEST_INIT_SEQUENCE::lastMonitoringServices = &services;
    TEST_INIT_SEQUENCE::lastMonitoringButtonHandler = &buttonHandler;
}

void MemoryConfig::printStats() {
    TEST_INIT_SEQUENCE::calls.push("memory.printStats");
    TEST_INIT_SEQUENCE::printStatsCalls++;
}

}  // namespace SYSTEM

namespace SYSTEM::NETWORK {

bool HttpRedirectServer::begin(const httpd_config_t& baseConfig) {
    TEST_INIT_SEQUENCE::calls.push("redirect.begin");
    TEST_INIT_SEQUENCE::redirectBeginCalls++;
    TEST_INIT_SEQUENCE::lastRedirectBaseConfig = &baseConfig;
    return TEST_INIT_SEQUENCE::redirectBeginResult;
}

void HttpRedirectServer::stop() {}

}  // namespace SYSTEM::NETWORK

namespace Utils {

bool JsonResponseWriter::begin() {
    TEST_INIT_SEQUENCE::calls.push("json.begin");
    TEST_INIT_SEQUENCE::jsonWriterBeginCalls++;
    return TEST_INIT_SEQUENCE::jsonWriterBeginResult;
}

}  // namespace Utils

namespace SYSTEM {

void BootTracker::begin() {
    TEST_INIT_SEQUENCE::calls.push("bootTracker.begin");
    TEST_INIT_SEQUENCE::bootTrackerBeginCalls++;
}

}  // namespace SYSTEM

void ServiceRegistry::begin(PsychicHttpServer& server,
                            ESP32SvelteKit& framework,
                            SemaphoreHandle_t networkMutex,
                            SemaphoreHandle_t notifMutex) {
    TEST_INIT_SEQUENCE::calls.push("registry.begin");
    TEST_INIT_SEQUENCE::registryBeginCalls++;
    TEST_INIT_SEQUENCE::lastBeginServer = &server;
    TEST_INIT_SEQUENCE::lastBeginFramework = &framework;
    TEST_INIT_SEQUENCE::lastBeginServices = this;
    TEST_INIT_SEQUENCE::lastNetworkMutex = networkMutex;
    TEST_INIT_SEQUENCE::lastNotifMutex = notifMutex;
}

void setUp(void) {
    TEST_INIT_SEQUENCE::reset();
}

void tearDown(void) {}

void test_phase2_logging_applies_saved_level_and_starts_boot_tracking() {
    TEST_INIT_SEQUENCE::configuredLevel = ESP_LOG_ERROR;

    SYSTEM::InitSequence::phase2_Logging();

    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::loggingBeginCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::loggingGetCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::loggingSetLevelCalls);
    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::bootTrackerBeginCalls);
    TEST_ASSERT_EQUAL_INT(ESP_LOG_ERROR, TEST_INIT_SEQUENCE::lastSetLevel);

    const std::vector<std::string> expected = {
        "loggingConfig.begin",
        "loggingConfig.get",
        "logging.setLevel",
        "bootTracker.begin",
    };
    TEST_ASSERT_EQUAL(expected.size(), TEST_INIT_SEQUENCE::calls.entries.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        TEST_ASSERT_EQUAL_STRING(expected[i].c_str(), TEST_INIT_SEQUENCE::calls.entries[i].c_str());
    }
}

void test_configure_network_applies_http_settings_to_http_and_https_servers() {
    PsychicHttpsServer server;

    SYSTEM::InitSequence::configureNetwork(server);

    TEST_ASSERT_EQUAL_UINT32(NET::HTTP::SERVER_TASK_PRIORITY, server.config.task_priority);
    TEST_ASSERT_EQUAL_INT(NET::HTTP::MAX_OPEN_SOCKETS, server.config.max_open_sockets);
    TEST_ASSERT_EQUAL_UINT32(NET::HTTP::BACKLOG_CONNECTIONS, server.config.backlog_conn);
    TEST_ASSERT_EQUAL_UINT32(NET::HTTP::MAX_REQUEST_HEADER_BYTES, server.config.max_req_hdr_len);
    TEST_ASSERT_EQUAL(NET::HTTP::LRU_PURGE_ENABLE, server.config.lru_purge_enable);
    TEST_ASSERT_EQUAL_INT(NET::HTTP::SESSION_TIMEOUT_SEC, server.config.recv_wait_timeout);
    TEST_ASSERT_EQUAL_INT(NET::HTTP::SESSION_TIMEOUT_SEC, server.config.send_wait_timeout);
    TEST_ASSERT_EQUAL_UINT32(NET::HTTP::SERVER_STACK_SIZE_BYTES, server.config.stack_size);
    TEST_ASSERT_EQUAL_INT(CONFIG::TASKS::CORE_PRO, server.config.core_id);

    TEST_ASSERT_EQUAL_UINT32(server.config.task_priority, server.ssl_config.httpd.task_priority);
    TEST_ASSERT_EQUAL_INT(server.config.max_open_sockets, server.ssl_config.httpd.max_open_sockets);
    TEST_ASSERT_EQUAL_UINT32(server.config.backlog_conn, server.ssl_config.httpd.backlog_conn);
    TEST_ASSERT_EQUAL_UINT32(server.config.max_req_hdr_len, server.ssl_config.httpd.max_req_hdr_len);
    TEST_ASSERT_EQUAL(server.config.lru_purge_enable, server.ssl_config.httpd.lru_purge_enable);
    TEST_ASSERT_EQUAL_INT(server.config.recv_wait_timeout, server.ssl_config.httpd.recv_wait_timeout);
    TEST_ASSERT_EQUAL_INT(server.config.send_wait_timeout, server.ssl_config.httpd.send_wait_timeout);
    TEST_ASSERT_EQUAL_UINT32(server.config.stack_size, server.ssl_config.httpd.stack_size);
    TEST_ASSERT_EQUAL_INT(server.config.core_id, server.ssl_config.httpd.core_id);
    TEST_ASSERT_EQUAL_UINT32(NET::HTTP::TLS_HANDSHAKE_TIMEOUT_MS,
                             server.ssl_config.tls_handshake_timeout_ms);
}

void test_phase4_framework_initializes_json_writer_before_framework_begin() {
    ESP32SvelteKit framework;

    SYSTEM::InitSequence::phase4_Framework(framework);

    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::jsonWriterBeginCalls);
    TEST_ASSERT_EQUAL_INT(1, framework.beginCalls);
    TEST_ASSERT_FALSE(framework.lastMountFs);
    TEST_ASSERT_NOT_NULL(framework.lastCert);
    TEST_ASSERT_NOT_NULL(framework.lastKey);

    const std::vector<std::string> expected = {
        "json.begin",
    };
    TEST_ASSERT_EQUAL(expected.size(), TEST_INIT_SEQUENCE::calls.entries.size());
    TEST_ASSERT_EQUAL_STRING(expected[0].c_str(), TEST_INIT_SEQUENCE::calls.entries[0].c_str());
}

void test_phase4_http_redirect_starts_with_configured_http_config() {
    SYSTEM::NETWORK::HttpRedirectServer redirectServer;
    httpd_config_t config{};
    config.max_open_sockets = 9;

    SYSTEM::InitSequence::phase4_HttpRedirect(redirectServer, config);

    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::redirectBeginCalls);
    TEST_ASSERT_EQUAL_PTR(&config, TEST_INIT_SEQUENCE::lastRedirectBaseConfig);
    TEST_ASSERT_EQUAL_STRING("redirect.begin", TEST_INIT_SEQUENCE::calls.entries[0].c_str());
}

void test_phase5_services_passes_server_framework_and_mutexes_to_registry() {
    PsychicHttpServer server;
    ESP32SvelteKit framework(&server);
    auto& services = TEST_INIT_SEQUENCE::rawObject<ServiceRegistry>();
    auto networkMutex = reinterpret_cast<SemaphoreHandle_t>(0x11);
    auto notifMutex = reinterpret_cast<SemaphoreHandle_t>(0x22);

    SYSTEM::InitSequence::phase5_Services(server, framework, services, networkMutex, notifMutex);

    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::registryBeginCalls);
    TEST_ASSERT_EQUAL_PTR(&server, TEST_INIT_SEQUENCE::lastBeginServer);
    TEST_ASSERT_EQUAL_PTR(&framework, TEST_INIT_SEQUENCE::lastBeginFramework);
    TEST_ASSERT_EQUAL_PTR(&services, TEST_INIT_SEQUENCE::lastBeginServices);
    TEST_ASSERT_EQUAL_PTR(networkMutex, TEST_INIT_SEQUENCE::lastNetworkMutex);
    TEST_ASSERT_EQUAL_PTR(notifMutex, TEST_INIT_SEQUENCE::lastNotifMutex);
}

void test_phase6_phase7_and_signal_boot_complete_delegate_to_runtime_components() {
    auto& services = TEST_INIT_SEQUENCE::rawObject<ServiceRegistry>();
    auto& buttonHandler = TEST_INIT_SEQUENCE::rawObject<ButtonHandler>();

    SYSTEM::InitSequence::phase6_Tasks(services);
    SYSTEM::InitSequence::phase7_Monitoring(services, buttonHandler);
    SYSTEM::InitSequence::signalBootComplete();

    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::runtimeTasksInitCalls);
    TEST_ASSERT_EQUAL_PTR(&services, TEST_INIT_SEQUENCE::lastRuntimeTasksServices);
    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::monitoringInitCalls);
    TEST_ASSERT_EQUAL_PTR(&services, TEST_INIT_SEQUENCE::lastMonitoringServices);
    TEST_ASSERT_EQUAL_PTR(&buttonHandler, TEST_INIT_SEQUENCE::lastMonitoringButtonHandler);
    TEST_ASSERT_EQUAL_INT(1, TEST_INIT_SEQUENCE::printStatsCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_phase2_logging_applies_saved_level_and_starts_boot_tracking);
    RUN_TEST(test_configure_network_applies_http_settings_to_http_and_https_servers);
    RUN_TEST(test_phase4_framework_initializes_json_writer_before_framework_begin);
    RUN_TEST(test_phase4_http_redirect_starts_with_configured_http_config);
    RUN_TEST(test_phase5_services_passes_server_framework_and_mutexes_to_registry);
    RUN_TEST(test_phase6_phase7_and_signal_boot_complete_delegate_to_runtime_components);
    return UNITY_END();
}
