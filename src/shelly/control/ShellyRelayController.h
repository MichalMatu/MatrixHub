#pragma once

#include <vector>
#include <functional>
#include <memory>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../ShellyTypes.h"
#include "../device/ShellyDeviceManager.h"
#include "../../system/network/UnifiedHttpClient.h"

namespace SHELLY {

class ShellyRelayController {
public:
    ShellyRelayController(ShellyDeviceManager& deviceManager,
                          std::atomic<bool>& runningFlag,
                          SemaphoreHandle_t networkMutex = nullptr);

    /**
     * Set relay state for a device.
     * @param id Device ID
     * @param turnOn true = ON, false = OFF
     * @return true if successful
     */
    bool setRelay(const char* id, bool turnOn);


    /**
     * Poll a single device for current state.
     * @param device Device to poll
     * @return true if device was online and updated
     */
    bool pollDevice(const ShellyDevice& device);

    /**
     * Release lazy-loaded resources (HTTP client, buffers).
     * Called by ShellyWorker when no devices exist to free memory.
     * Public because worker manages polling lifecycle externally.
     * Thread-safe: uses network mutex internally.
     */
    void releaseResources();

    /**
     * Break an in-flight HTTP/TLS request without freeing the controller state.
     * Used by ShellyWorker::stop() so shutdown/restart is not forced to wait
     * for the full network timeout budget when a device is slow or unreachable.
     */
    void cancelActiveIo();

private:
    ShellyDeviceManager& _deviceManager;
    
    // Lazy loaded resources - now using UnifiedHttpClient (Golden Standard)
    std::unique_ptr<NETWORK::UnifiedHttpClient> _httpClient;
    char* _responseBuffer = nullptr;
    
    std::atomic<bool>& _running;
    SemaphoreHandle_t _networkMutex;

    bool ensureResources();
};

} // namespace SHELLY
