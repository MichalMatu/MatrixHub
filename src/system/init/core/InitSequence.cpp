/**
 * @file InitSequence.cpp
 * @brief Phased initialization sequence implementation
 * 
 * Extracted from Application::setup() for better modularity.
 */

#include "InitSequence.h"
#include "MemoryConfig.h"
#include "../services/MonitoringInitializer.h"
#include "../services/PowerInitializer.h"
#include "../services/RuntimeTasksInitializer.h"
#include "../services/StorageInitializer.h"
#include "../../services/ServiceRegistry.h"
#include "../../../config/System.h"
#include "../../../config/certificates.h"
#include "../../../config/App.h"
#include "../../logging/Logging.h"
#include "../../network/HttpRedirectServer.h"
#include "../../boot/BootTracker.h"
#include "../../utils/json/JsonResponseWriter.h"
#include <PsychicHttpsServer.h>
#include <cstdlib>

#undef LOG_TAG
#define LOG_TAG "Init"

namespace SYSTEM {

namespace {

void configureHttpServer(httpd_config_t& config) {
    config.task_priority = NET::HTTP::SERVER_TASK_PRIORITY;
    config.max_open_sockets = NET::HTTP::MAX_OPEN_SOCKETS;
    config.backlog_conn = NET::HTTP::BACKLOG_CONNECTIONS;
    config.max_req_hdr_len = NET::HTTP::MAX_REQUEST_HEADER_BYTES;
    config.lru_purge_enable = NET::HTTP::LRU_PURGE_ENABLE;
    config.recv_wait_timeout = NET::HTTP::SESSION_TIMEOUT_SEC;
    config.send_wait_timeout = NET::HTTP::SESSION_TIMEOUT_SEC;
    config.stack_size = NET::HTTP::SERVER_STACK_SIZE_BYTES;
    config.core_id = CONFIG::TASKS::CORE_PRO;
}

void logNetworkConfiguration(const httpd_config_t& config) {
    LOGI("[Network] Configured HTTP Server: prio=%u sockets=%d backlog=%u hdr=%u timeout=%d core=%d",
         static_cast<unsigned>(config.task_priority),
         config.max_open_sockets,
         static_cast<unsigned>(config.backlog_conn),
         static_cast<unsigned>(config.max_req_hdr_len),
         config.recv_wait_timeout,
         static_cast<int>(config.core_id));
}

}  // namespace

/*
 * INITIALIZATION DEPENDENCY GRAPH
 * 
 * Phase 1: Storage & Synch
 * - NVS/FS: Required for reading config used by later phases.
 * - Mutexes: created EARLY to be available when hardware services start (Phase 3).
 * 
 * Phase 2: Logging & Diag
 * - Logging: Needs FS (if file logging) and config.
 * - BootTracker: Captures RTC-only boot markers before we do anything risky.
 * 
 * Phase 3: Hardware & Power
 * - Watchdog: Started early to catch hangs during hardware init.
 * - PowerManager: Initializes PMU/sleep logic.
 * - I2C/IMU/Matrix: Hardware drivers.
 * 
 * Phase 4: Network/Framework
 * - Needs Storage (Phase 1) for certs/config.
 * 
 * Phase 5: Services
 * - Instantiates high-level logic.
 * 
 * Phase 6: Tasks
 * - Starts FreeRTOS tasks that use services from Phase 5.
 * 
 * Phase 7: Monitoring
 * - Watchdogs, Health checks.
 */
void InitSequence::phase1_Storage() {
    StorageInitializer::initialize();
}

void InitSequence::phase2_Logging() {
    LoggingConfig::begin();
    auto logCfg = LoggingConfig::get();
    // Re-apply the saved level after Phase 1 in case runtime config changed
    // while the boot loader was still using the emergency startup level.
    LOG::Logging::setLevel(logCfg.level);

    LOGI("\n\n* * * %s %s v%s * * *\n", APP::DEVICE, APP::NAME, APP::VERSION);
    
    LOGI("[Phase2] Logging: level=%s", 
         LOG::Logging::levelToString(logCfg.level));

    // Initialize boot tracking early to detect unexpected restarts
    BootTracker::begin();
}

void InitSequence::phase3_Power(ServiceRegistry& services) {
    PowerInitializer::initialize(services);
}

void InitSequence::configureNetwork(PsychicHttpsServer& server) {
    // Increase max open sockets to handle Asset Loading + WebSocket + API
    // Must be set BEFORE framework.begin() / server.listen()
    configureHttpServer(server.config);
    
    // Keep HTTPS httpd pinned off CORE_APP as well, otherwise the secure
    // server task can preempt loopTask and distort wall-time diagnostics.
    configureHttpServer(server.ssl_config.httpd);
    server.ssl_config.tls_handshake_timeout_ms = NET::HTTP::TLS_HANDSHAKE_TIMEOUT_MS;

    logNetworkConfiguration(server.config);
    LOGI("[Network] HTTPS handshake timeout: %lu ms",
         static_cast<unsigned long>(server.ssl_config.tls_handshake_timeout_ms));
         
    // Note: Noisy SSL/HTTP logs suppressed centrally in Logging::suppressNoisyModules()
}

void InitSequence::phase4_Framework(ESP32SvelteKit& framework) {
    // Initialize PSRAM buffer for JSON streaming before any HTTP handlers run
    if (!Utils::JsonResponseWriter::begin()) {
        LOGE("[Phase4] Failed to initialize JsonResponseWriter PSRAM buffer");
        std::abort();
    }
    
    // StorageInitializer is the only LittleFS owner.
    // Framework must reuse the Phase 1 mount instead of remounting/repairing FS.
    framework.begin(server_cert, server_key, BOOT::FRAMEWORK::MOUNT_FILESYSTEM);
    LOGI("[Phase4] Framework initialized");
}

void InitSequence::phase4_HttpRedirect(NETWORK::HttpRedirectServer& redirectServer,
                                       const httpd_config_t& baseConfig) {
    if (!redirectServer.begin(baseConfig)) {
        LOGW("[Phase4] HTTP redirect server unavailable; HTTPS remains active");
    }
}

void InitSequence::phase5_Services(PsychicHttpServer& server, 
                                    ESP32SvelteKit& framework,
                                    ServiceRegistry& services,
                                    SemaphoreHandle_t networkMutex,
                                    SemaphoreHandle_t notifMutex) {
    services.begin(server, framework, networkMutex, notifMutex);
    LOGI("[Phase5] Services initialized");
}

void InitSequence::phase6_Tasks(ServiceRegistry& services) {
    RuntimeTasksInitializer::initialize(services);
}

void InitSequence::phase7_Monitoring(ServiceRegistry& services, ButtonHandler& buttonHandler) {
    MonitoringInitializer::initialize(services, buttonHandler);
}

void InitSequence::signalBootComplete() {
    LOGI("Setup complete - System running");
    MemoryConfig::printStats();
}

}  // namespace SYSTEM
