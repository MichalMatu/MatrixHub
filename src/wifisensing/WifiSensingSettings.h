/**
 * @file WifiSensingSettings.h
 * @brief Persistent settings for WiFi Sensing module - backed by RTC memory
 * 
 * Refactored 3 Jan 2026: Uses centralized ConfigManager for persistence.
 */

#pragma once

#include "../system/rtc/RtcStatefulService.h"
#include "../system/rtc/RtcConfig.h"
#include "../config/App.h"
#include "core/config/ConfigManager.h"
#include "core/WifiRuntimeConfigFence.h"
#include "core/WifiRuntimeReconcileSchedule.h"
#include "core/WifiRuntimeReconcileWorkerGate.h"

namespace WIFISENSING {

class WifiSensingService;
namespace CSI {
class CsiService;
}

class WifiSensingSettings : public RtcStatefulService<RTC::WifiSensingData> {
public:
    WifiSensingSettings(FS* fs,
                        WIFISENSING::WifiSensingService* service,
                        WIFISENSING::CSI::CsiService* csiService);
    ~WifiSensingSettings() override;

    void begin();
    void reconcileRuntimeIfDue(uint32_t nowMs);
    bool shutdownRuntimeReconciler(
        TickType_t waitTicks = TIMEOUT::TASK_SHUTDOWN_TICKS);
    static void readState(RTC::WifiSensingData& settings, JsonObject& root);
    static StateUpdateResult updateState(JsonObject& jsonObject, RTC::WifiSensingData& settings, std::string_view originId);

    bool isEnabled() const;
    uint16_t getSampleIntervalMs() const;
    float getVarianceThreshold() const;

private:
    StateHandlerResult onConfigUpdated();
    void runRuntimeReconcileAttempt();
    bool isRuntimeReconcileDue(uint32_t nowMs);
    void markRuntimeReconcileHealthy(uint32_t completedAtMs);
    void markRuntimeReconcileFailure(uint32_t completedAtMs);
    bool ensureRuntimeReconcileWorkerLocked();
    bool reapRuntimeReconcileWorkerLocked(TickType_t waitTicks);
    static void runtimeReconcileTask(void* context);
    
    FS* _fs;
    WIFISENSING::WifiSensingService* _service; // Injected
    WIFISENSING::CSI::CsiService* _csiService; // Injected
    SemaphoreHandle_t _runtimeApplyMutex = nullptr;
    SemaphoreHandle_t _runtimeReconcileLifecycleMutex = nullptr;
    SemaphoreHandle_t _runtimeReconcileStopped = nullptr;
    TaskHandle_t _runtimeReconcileTaskHandle = nullptr;
    WifiRuntimeConfigFence _runtimeConfigFence;
    RTC::WifiSensingData _lastPersistedState{};
    portMUX_TYPE _runtimeReconcileScheduleMux = portMUX_INITIALIZER_UNLOCKED;
    WifiRuntimeReconcileSchedule _runtimeReconcileSchedule;
    WifiRuntimeReconcileWorkerGate _runtimeReconcileWorkerGate;
};

}  // namespace WIFISENSING
