#pragma once

#include <PsychicHttpServer.h>
#include <core/ESP32SvelteKit.h>

class ServiceRegistry;
class PsychicHttpsServer;
class ButtonHandler;

namespace SYSTEM {

namespace NETWORK {
class HttpRedirectServer;
}

/**
 * @brief Phased initialization sequence for Application boot
 * 
 * Extracted from Application::setup() for better testability and clarity.
 *
 * The phase order here is the canonical boot contract:
 * - storage/RTC first
 * - logging and power before framework/services
 * - services before background tasks
 * - monitoring/button wiring last, once all side-effect owners exist
 *
 * Keep high-level boot reasoning here and in Application.cpp so the main
 * startup path remains debuggable without chasing multiple engineering docs.
 */
class InitSequence {
public:
    // Phase 1: Storage initialization (NVS, LittleFS, RTC memory)
    static void phase1_Storage();
    
    // Phase 2: Logging system (LoggingConfig, RingBuffer, BootTracker)
    static void phase2_Logging();
    
    // Phase 3: Power management and LED indicator
    static void phase3_Power(class ServiceRegistry& services);

    // Helper: Verify and apply network stack configuration
    // (Must run before Framework initialization)
    static void configureNetwork(PsychicHttpsServer& server);
    
    // Phase 4: ESP32SvelteKit framework
    static void phase4_Framework(ESP32SvelteKit& framework);

    // Phase 4b: Plain HTTP convenience redirect to the HTTPS UI/API server.
    static void phase4_HttpRedirect(NETWORK::HttpRedirectServer& redirectServer,
                                    const httpd_config_t& baseConfig);
    
    // Phase 5: Business and API services
    static void phase5_Services(PsychicHttpServer& server, 
                                ESP32SvelteKit& framework,
                                ServiceRegistry& services,
                                SemaphoreHandle_t networkMutex,
                               SemaphoreHandle_t notifMutex);
    
    // Phase 6: Background tasks (DataLogger, Alarms, Heartbeat, Sensors)
    static void phase6_Tasks(class ServiceRegistry& services);
    
    // Phase 7: System monitoring (Button, Heap, Watchdog, Health)
    // ButtonHandler arrives from Application because input ownership moved out
    // of singleton/global space and is now part of the main runtime object.
    static void phase7_Monitoring(class ServiceRegistry& services, class ButtonHandler& buttonHandler);
    
    // Final: Boot complete signal (LED flash)
    static void signalBootComplete();
};

}  // namespace SYSTEM
