#pragma once

#include <memory>
#include <functional>
#include <FS.h>
#include <Arduino.h>

namespace SHELLY {

class ShellyService;

// Tiny lifecycle seam around ShellyService. The registry uses this to test
// boot/shutdown wiring without dragging the full Shelly implementation into
// every native translation unit that only cares about orchestration.
void ensureOwnedService(std::unique_ptr<ShellyService>& slot,
                        FS& fs,
                        SemaphoreHandle_t networkMutex);
void loadConfig(ShellyService* service);
size_t deviceCount(const ShellyService* service);
void beginService(ShellyService* service);
void reconcileRuntimeIfDue(ShellyService* service, uint32_t nowMs);
void setConfigChangeCallback(ShellyService* service,
                             std::function<void()> callback);
bool runtimeIsRunning(const ShellyService* service);
void runtimeStop(ShellyService* service);

}  // namespace SHELLY
