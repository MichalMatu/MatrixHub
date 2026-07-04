#include "ShellyRelayController.h"
#include "../device/ShellyDeviceManager.h"
#include "../protocol/ShellyProtocol.h"
#include "../validation/IpValidator.h"
#include "../../system/logging/Logging.h"
#include <esp_heap_caps.h>
#include <WiFi.h>
#include "../../system/utils/ScopeLock.h"
#include "../../config/System.h"  // TIMEOUT::

#undef LOG_TAG
#define LOG_TAG "ShellyCtrl"

namespace SHELLY {

namespace {

bool buildControlUrlForGeneration(uint8_t generation, char* buffer, size_t size,
                                  const char* ip, uint8_t relayIndex, bool turnOn) {
    if (generation == 1) {
        return ShellyProtocol::buildGen1ControlUrl(buffer, size, ip, relayIndex, turnOn);
    }
    return ShellyProtocol::buildGen2ControlUrl(buffer, size, ip, relayIndex, turnOn);
}

bool buildStatusUrlForGeneration(uint8_t generation, char* buffer, size_t size, const char* ip) {
    if (generation == 1) {
        return ShellyProtocol::buildGen1StatusUrl(buffer, size, ip);
    }
    return ShellyProtocol::buildGen2StatusUrl(buffer, size, ip);
}

bool parseStatusForGeneration(uint8_t generation, char* jsonResponse, size_t length,
                              uint8_t relayIndex, ShellyStatus& outStatus) {
    if (generation == 1) {
        return ShellyProtocol::parseGen1Status(jsonResponse, length, relayIndex, outStatus);
    }
    return ShellyProtocol::parseGen2Status(jsonResponse, length, relayIndex, outStatus);
}

uint8_t alternateGeneration(uint8_t generation) {
    return (generation == 1) ? 2 : 1;
}

} // namespace

ShellyRelayController::ShellyRelayController(ShellyDeviceManager& deviceManager,
                                             std::atomic<bool>& runningFlag,
                                             SemaphoreHandle_t networkMutex)
    : _deviceManager(deviceManager)
    , _running(runningFlag)
    , _networkMutex(networkMutex) {
}

bool ShellyRelayController::ensureResources() {
    if (!_httpClient) {
        LOGD("Lazy Loading: Allocating UnifiedHttpClient (DRY)");
        _httpClient = std::make_unique<NETWORK::UnifiedHttpClient>(&_running);
        // Shelly devices (especially Gen 1) work better without connection reuse
        _httpClient->setReuse(false);
    }
    
    if (!_responseBuffer) {
         LOGD("Lazy Loading: Allocating Response Buffer (PSRAM)");
         _responseBuffer = (char*)heap_caps_malloc(4096, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
         if (!_responseBuffer) {
             LOGE("Failed to allocate PSRAM buffer for Shelly");
         }
    }
    
    return _httpClient && _responseBuffer;
}

void ShellyRelayController::releaseResources() {
    if (!_httpClient && !_responseBuffer) return;
    
    LOGD("Auto-Release: Freeing Shelly resources (idle)");
    _httpClient.reset();
    if (_responseBuffer) {
        heap_caps_free(_responseBuffer);
        _responseBuffer = nullptr;
    }
}

void ShellyRelayController::cancelActiveIo() {
    if (!_httpClient) {
        return;
    }

    // Mirror the shutdown strategy already used by Heartbeat and notification
    // transports: tear down the raw socket/TLS I/O immediately, but leave the
    // higher-level client object alive so the worker can finish unwinding on its
    // normal serialized path instead of racing a use-after-free.
    _httpClient->cancelActiveIo();
}

bool ShellyRelayController::setRelay(const char* id, bool turnOn) {
    // 1. Get device copy
    ShellyDevice device;
    if (!_deviceManager.getDevice(id, device)) {
        LOGW("setRelay: Device not found: %s", id);
        return false;
    }

    if (!device.enabled) {
        LOGW("setRelay: Device disabled: %s", id);
        return false;
    }

    // 2. Validate IP
    if (!IpValidator::isValidPrivateIp(device.ip)) {
        LOGE("Invalid IP: %s", device.ip);
        return false;
    }

    // 1. URL Building (OUTSIDE MUTEX)
    char url[128];
    if (!ensureResources()) {
        return false;
    }

    bool success = false;
    // Control timeout can be slightly longer to ensure the signal arrives
    constexpr uint16_t kSetRelayTimeoutMs = 800;

    auto trySetRelay = [&](uint8_t generation) -> bool {
        if (!buildControlUrlForGeneration(generation, url, sizeof(url), device.ip, device.relayIndex, turnOn)) {
            LOGE("Failed to build control URL for %s (gen=%u)", id, generation);
            return false;
        }

        SYSTEM::ScopeLock lock(_networkMutex, TIMEOUT::MUTEX_NETWORK_TICKS);
        if (!lock.isLocked()) {
            LOGW("setRelay: Network mutex timeout for %s", id);
            return false;
        }

        return _httpClient->get(url, nullptr, 0, kSetRelayTimeoutMs);
    };

    success = trySetRelay(device.generation);
    if (!success) {
        const uint8_t fallbackGeneration = alternateGeneration(device.generation);
        if (trySetRelay(fallbackGeneration)) {
            success = true;
            LOGW("Auto-corrected Shelly generation for %s: %u -> %u",
                 id, device.generation, fallbackGeneration);

            ShellyDevice corrected = device;
            corrected.generation = fallbackGeneration;
            if (!_deviceManager.upsertDevice(corrected)) {
                LOGE("Failed to persist corrected Shelly generation for %s", id);
            }
        }
    }

    if (success) {
        _deviceManager.updateCommandState(id, turnOn, true);
    }

    if (success) LOGI("Relay %s -> %s", id, turnOn ? "ON" : "OFF");
    else LOGE("Failed: %s", id);

    return success;
}

bool ShellyRelayController::pollDevice(const ShellyDevice& t) {
    if (!_running.load()) return false;
    
    // Skip disabled devices
    if (!t.enabled) return false;
    
    // Network check is done by caller (ShellyWorker) and by UnifiedHttpClient

    // Using member _httpClient instead of local instance
    // No global mutex needed - serialized by ShellyWorker

    if (!ensureResources()) {
        LOGE("Failed to allocate resources for Shelly polling");
        return false;
    }

    // 1. URL Building
    char urlBuffer[128];
    bool online = false;
    ShellyStatus status;
    uint8_t detectedGeneration = t.generation;
    
    LOGV_THROTTLED(TASK_MONITOR::INTERVAL_SHELLY_POLL_MS, "Polling %s (%s)", t.id, t.ip);

    auto tryPollGeneration = [&](uint8_t generation, ShellyStatus& outStatus) -> bool {
        size_t bytesRead = 0;
        if (!buildStatusUrlForGeneration(generation, urlBuffer, sizeof(urlBuffer), t.ip)) {
            LOGE("Failed to build status URL for %s (gen=%u)", t.id, generation);
            return false;
        }

        // No mutex needed — polling is serialized by ShellyWorker task.
        if (!_httpClient->poll(urlBuffer, _responseBuffer, 4096, 3000, &bytesRead)) {
            return false;
        }

        LOGV_THROTTLED(TASK_MONITOR::INTERVAL_SHELLY_POLL_MS, "Response: %u bytes", bytesRead);
        if (!parseStatusForGeneration(generation, _responseBuffer, bytesRead, t.relayIndex, outStatus)) {
            LOGW("Parse Failed (Gen %u): %s", generation, t.id);
            LOGV("Raw [First 64B]: %.64s", _responseBuffer);
            return false;
        }

        return true;
    };

    online = tryPollGeneration(t.generation, status);
    if (!online) {
        const uint8_t fallbackGeneration = alternateGeneration(t.generation);
        ShellyStatus fallbackStatus;
        if (tryPollGeneration(fallbackGeneration, fallbackStatus)) {
            status = fallbackStatus;
            detectedGeneration = fallbackGeneration;
            online = true;
            LOGW("Auto-corrected Shelly generation for %s: %u -> %u",
                 t.id, t.generation, fallbackGeneration);

            ShellyDevice corrected = t;
            corrected.generation = fallbackGeneration;
            if (!_deviceManager.upsertDevice(corrected)) {
                LOGE("Failed to persist corrected Shelly generation for %s", t.id);
            }
        } else {
            const uint8_t nextFailureCount = static_cast<uint8_t>(t.failedPolls + 1);
            // Wrong/missing peers are common during setup and while devices are
            // being readdressed on the LAN. Summarize the whole failed poll once
            // after both generation attempts instead of emitting one warning per
            // attempt plus the underlying HTTP stack noise.
            if (t.isOnline || t.failedPolls == 0) {
                LOGW("Poll failed: %s ip=%s configuredGen=%u fallbackGen=%u failures=%u",
                     t.id, t.ip, t.generation, fallbackGeneration, nextFailureCount);
            } else if (nextFailureCount == 3) {
                LOGI("Shelly %s remains unreachable at %s after %u failed polls; reducing log noise while backoff grows",
                     t.id, t.ip, nextFailureCount);
            } else {
                LOGD_THROTTLED(TASK_MONITOR::INTERVAL_SHELLY_POLL_MS,
                               "Poll still failing: %s ip=%s failures=%u",
                               t.id,
                               t.ip,
                               nextFailureCount);
            }
        }
    }

    if (online) {
        LOGD_THROTTLED(TASK_MONITOR::INTERVAL_SHELLY_POLL_MS,
                       "Poll success: %s (IP: %s, Gen: %u, State: %s)",
                       t.id,
                       t.ip,
                       detectedGeneration,
                       status.isOn ? "ON" : "OFF");

        // Fix for Shelly intermittent 0.0W bug:
        // Shelly devices sometimes report apower=0.0 while ON, even with normal voltage.
        // We debounce: only accept 0W after kZeroPowerThreshold consecutive zero readings.
        constexpr uint8_t kZeroPowerThreshold = 3;

        if (status.isOn && status.hasPower && status.power == 0.0f) {
            ShellyDevice oldDevice;
            if (_deviceManager.getDevice(t.id, oldDevice) && oldDevice.power > 0.0f) {
                uint8_t count = oldDevice.zeroPowerCount + 1;
                if (count < kZeroPowerThreshold) {
                    LOGW("Shelly %s: 0W while ON (%u/%u), keeping %.1fW",
                         t.id, count, kZeroPowerThreshold, oldDevice.power);
                    status.power = oldDevice.power;
                    _deviceManager.withDevice(t.id, [count](ShellyDevice& d) {
                        d.zeroPowerCount = count;
                        return true;
                    });
                } else {
                    LOGI("Shelly %s: 0W confirmed after %u readings, accepting", t.id, count);
                    _deviceManager.withDevice(t.id, [](ShellyDevice& d) {
                        d.zeroPowerCount = 0;
                        return true;
                    });
                }
            }
        } else if (status.hasPower && status.power > 0.0f) {
            // Normal reading, reset counter
            _deviceManager.withDevice(t.id, [](ShellyDevice& d) {
                d.zeroPowerCount = 0;
                return true;
            });
        }
    }

    // 4. State update (has its own fast internal mutex)
    _deviceManager.updatePollState(t.id, status, online);
    
    return online;
}

} // namespace SHELLY
