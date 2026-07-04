#include "ShellyRuntimeControl.h"

#include "ShellyService.h"

namespace SHELLY {

// Keep these wrappers behaviorally minimal. They exist so orchestration code
// can expose each lifecycle step to tests and debugging without duplicating
// Shelly-specific policy in the registry.

void ensureOwnedService(std::unique_ptr<ShellyService>& slot,
                        FS& fs,
                        SemaphoreHandle_t networkMutex) {
    if (!slot) {
        slot = std::make_unique<ShellyService>(fs, networkMutex);
    }
}

void loadConfig(ShellyService* service) {
    if (service) {
        service->loadConfig();
    }
}

size_t deviceCount(const ShellyService* service) {
    // ShellyService still exposes getDeviceCount() as non-const. Keep the
    // const_cast boxed in this adapter so the rest of the init path can stay
    // const-correct and future cleanup has one obvious place to revisit.
    return service ? const_cast<ShellyService*>(service)->getDeviceCount() : 0;
}

void beginService(ShellyService* service) {
    if (service) {
        service->begin();
    }
}

bool runtimeIsRunning(const ShellyService* service) {
    return service && service->isRunning();
}

void runtimeStop(ShellyService* service) {
    if (service) {
        service->stop();
    }
}

}  // namespace SHELLY
