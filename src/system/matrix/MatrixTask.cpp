#include "MatrixTask.h"
#include "../../config/System.h"
#include "MatrixService.h"
#include "../matrix_manager/MatrixManagerService.h"
#include "../rtc/RtcConfig.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "../../sensors/imu/ImuService.h"
#include "../../sensors/imu/ImuManager.h"
#include "../../sensors/imu/ImuTypes.h"
#include "../../sensors/runtime/SensorState.h"
#include "../../sensors/runtime/SensorSnapshotHealth.h"
#include "../../ble/BleService.h"
#include "../../wifisensing/WifiSensingService.h"
#include "../../wifisensing/csi/core/CsiService.h"
#include "../watchdog/TaskWatchdog.h"
#include "../../matrix/menu/MatrixMenuService.h"
#include "../../matrix/MatrixDataVisualizationTypes.h"
#include "../../../lib/matrix_service/effects/MatrixFxTypes.h"
#include "../../system/logging/Logging.h"

#include <atomic>
#include <cmath>
#include <cstring>
#include <esp_wifi.h>
#include <WiFi.h>
#undef LOG_TAG
#define LOG_TAG "MatrixTask"

namespace MATRIX {

std::atomic<TaskHandle_t> MatrixTask::_taskHandle(nullptr);
StackType_t* MatrixTask::_taskStack = nullptr;
StaticTask_t* MatrixTask::_taskBuffer = nullptr;
SemaphoreHandle_t MatrixTask::_stopAck = nullptr;
std::atomic<bool> MatrixTask::_isRunning(false);
std::atomic<bool> MatrixTask::_lifecycleInProgress(false);
std::atomic<bool> MatrixTask::_shutdownEpilogueComplete(false);
uint32_t MatrixTask::_lastImuCheckMs = 0;
bool MatrixTask::_lastAutoRotateEnabled = false;
uint8_t MatrixTask::_lastAppliedAutoRotation = 0xFF;
bool MatrixTask::_lastMatrixEffectsImuEnabled = false;
bool MatrixTask::_lastMatrixDataVizCsiEnabled = false;
uint32_t MatrixTask::_lastDataVisualizationInputMs = 0;

namespace {

bool waitForTaskSuspended(TaskHandle_t taskHandle, SemaphoreHandle_t stopAck, TickType_t waitTicks) {
    if (taskHandle == nullptr) {
        return true;
    }

    if (eTaskGetState(taskHandle) == eSuspended) {
        return true;
    }

    if (stopAck != nullptr) {
        if (xSemaphoreTake(stopAck, waitTicks) != pdTRUE &&
            eTaskGetState(taskHandle) != eSuspended) {
            return false;
        }
    } else if (waitTicks > 0) {
        return false;
    }

    const TickType_t pollStep = pdMS_TO_TICKS(10) > 0 ? pdMS_TO_TICKS(10) : 1;
    const TickType_t settleWait = pdMS_TO_TICKS(50);
    TickType_t waited = 0;

    while (eTaskGetState(taskHandle) != eSuspended && waited < settleWait) {
        vTaskDelay(pollStep);
        waited += pollStep;
    }

    return eTaskGetState(taskHandle) == eSuspended;
}

class LifecycleOwnershipRelease {
public:
    explicit LifecycleOwnershipRelease(std::atomic<bool>& lifecycleInProgress)
        : _lifecycleInProgress(lifecycleInProgress) {}

    ~LifecycleOwnershipRelease() {
        _lifecycleInProgress.store(false, std::memory_order_release);
    }

private:
    std::atomic<bool>& _lifecycleInProgress;
};

float clampf(float value, float minValue, float maxValue) {
    if (!std::isfinite(value)) {
        return minValue;
    }
    if (value < minValue) {
        return minValue;
    }
    if (value > maxValue) {
        return maxValue;
    }
    return value;
}

float rssiQuality(int8_t rssi) {
    return clampf((static_cast<float>(rssi) + 100.0f) * 2.0f, 0.0f, 100.0f);
}

bool hasScd4xReading(const SensorSnapshot& snapshot) {
    return snapshot.co2 >= SENSOR::SCD4X::CO2_MIN_PPM &&
           snapshot.co2 <= SENSOR::SCD4X::CO2_MAX_PPM &&
           std::isfinite(snapshot.temp) &&
           std::isfinite(snapshot.humid) &&
           snapshot.humid >= SENSOR::SCD4X::HUMID_MIN_PCT &&
           snapshot.humid <= SENSOR::SCD4X::HUMID_MAX_PCT;
}

uint8_t normalizedByte(float value, float minValue, float maxValue) {
    if (maxValue <= minValue) {
        maxValue = minValue + 1.0f;
    }
    const float scaled = clampf((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
    return static_cast<uint8_t>(std::lround(scaled * 255.0f));
}

MATRIX::MatrixDataVisualizationConfig buildDataVisualizationConfig(const RTC::MatrixData& matrixConfig) {
    MATRIX::MatrixDataVisualizationConfig config;
    config.enabled = matrixConfig.dataVisualizationEnabled;
    config.source = MATRIX::normalizeMatrixDataSource(matrixConfig.dataVisualizationSource);
    config.metric = MATRIX::normalizeMatrixDataMetric(matrixConfig.dataVisualizationMetric);
    config.mode = MATRIX::normalizeMatrixDataVizMode(matrixConfig.dataVisualizationMode);
    config.minValue = matrixConfig.dataVisualizationMin;
    config.maxValue = matrixConfig.dataVisualizationMax > matrixConfig.dataVisualizationMin
        ? matrixConfig.dataVisualizationMax
        : matrixConfig.dataVisualizationMin + 1.0f;
    config.colorMin = MATRIX::normalizeMatrixDataColor(matrixConfig.dataVisualizationColorMin);
    config.colorMid = MATRIX::normalizeMatrixDataColor(matrixConfig.dataVisualizationColorMid);
    config.colorMax = MATRIX::normalizeMatrixDataColor(matrixConfig.dataVisualizationColorMax);
    config.brightnessMin = matrixConfig.dataVisualizationBrightnessMin;
    config.brightnessMax = matrixConfig.dataVisualizationBrightnessMax;
    if (config.brightnessMax < config.brightnessMin) {
        const uint8_t tmp = config.brightnessMax;
        config.brightnessMax = config.brightnessMin;
        config.brightnessMin = tmp;
    }
    config.smoothing = matrixConfig.dataVisualizationSmoothing;
    config.staleBehavior = MATRIX::normalizeMatrixDataStaleBehavior(matrixConfig.dataVisualizationStaleBehavior);
    MATRIX::copyMatrixDataDeviceId(config.deviceId, sizeof(config.deviceId), matrixConfig.dataVisualizationDeviceId);
    MATRIX::normalizeMatrixDataVisualizationConfig(config);
    return config;
}

struct RssiBinsResult {
    uint16_t count = 0;
    uint32_t newestTimestampMs = 0;
};

struct DirectRssiResult {
    bool wifiActive = false;
    bool valid = false;
    int8_t rssi = 0;
};

uint32_t dataVisualizationInputIntervalMs(const MATRIX::MatrixDataVisualizationConfig& config) {
    if (config.source == static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi)) {
        return UI::MATRIX::DATA_VISUALIZATION_CSI_INPUT_INTERVAL_MS;
    }
    return UI::MATRIX::DATA_VISUALIZATION_INPUT_INTERVAL_MS;
}

uint32_t wifiRssiStaleTimeoutMs(uint32_t sampleIntervalMs) {
    const uint32_t intervalBased = sampleIntervalMs * 3u + UI::MATRIX::DATA_VISUALIZATION_INPUT_INTERVAL_MS;
    return intervalBased > API::SENSOR_DATA_WAIT_MS ? intervalBased : API::SENSOR_DATA_WAIT_MS;
}

RssiBinsResult fillRssiBins(WIFISENSING::WifiSensingService* wifiSensingService,
                            MATRIX::MatrixDataVisualizationInput& input) {
    RssiBinsResult result;
    if (!wifiSensingService) {
        return result;
    }
    WIFISENSING::RssiSample samples[MATRIX::kMatrixDataVizPixelCount];
    const uint16_t count = wifiSensingService->getSamples(samples, MATRIX::kMatrixDataVizPixelCount);
    if (count == 0) {
        return result;
    }

    input.binCount = static_cast<uint8_t>(count > MATRIX::kMatrixDataVizPixelCount
        ? MATRIX::kMatrixDataVizPixelCount
        : count);
    result.count = input.binCount;
    result.newestTimestampMs = samples[0].timestampMs;
    for (uint8_t i = 0; i < input.binCount; ++i) {
        const uint8_t sourceIndex = static_cast<uint8_t>(input.binCount - 1u - i);
        input.bins[i] = normalizedByte(rssiQuality(samples[sourceIndex].rssi), 0.0f, 100.0f);
    }
    return result;
}

DirectRssiResult readDirectRssi() {
    DirectRssiResult result;
    const wifi_mode_t mode = WiFi.getMode();

    if (WiFi.isConnected()) {
        result.wifiActive = true;
        result.valid = true;
        result.rssi = WiFi.RSSI();
        return result;
    }

    const bool apMode = mode == WIFI_AP || mode == WIFI_AP_STA;
    if (!apMode) {
        return result;
    }

    const int stationCount = WiFi.softAPgetStationNum();
    if (stationCount <= 0) {
        return result;
    }

    result.wifiActive = true;
    wifi_sta_list_t stations{};
    if (esp_wifi_ap_get_sta_list(&stations) == ESP_OK && stations.num > 0) {
        result.valid = true;
        result.rssi = stations.sta[0].rssi;
    }
    return result;
}

void fillCsiBins(const WIFISENSING::CSI::CsiVisualizationSnapshot& snapshot,
                 MATRIX::MatrixDataVisualizationInput& input) {
    input.binCount = snapshot.binCount > MATRIX::kMatrixDataVizPixelCount
        ? MATRIX::kMatrixDataVizPixelCount
        : snapshot.binCount;
    for (uint8_t i = 0; i < input.binCount; ++i) {
        input.bins[i] = snapshot.bins[i];
    }
}

} // namespace

void MatrixTask::start(MatrixMenuService* menu,
                       ImuService* imuService,
                       IMU::ImuManager* imuManager,
                       MatrixService* matrixService,
                       MATRIX_MANAGER::MatrixManagerService* matrixManager,
                       BLE::BleService* bleService,
                       WIFISENSING::WifiSensingService* wifiSensingService,
                       WIFISENSING::CSI::CsiService* csiService) {
    if (!acquireLifecycleOwnership(0)) {
        LOGW("Cannot start MatrixTask during another lifecycle transition");
        return;
    }
    LifecycleOwnershipRelease releaseLifecycleOwnership(_lifecycleInProgress);

    if (!matrixService) {
        LOGE("Cannot start MatrixTask without MatrixService");
        return;
    }

    if (_taskHandle.load(std::memory_order_acquire)) {
        if (!_isRunning.load()) {
            (void)reapStoppedTask(0);
        }
        if (_taskHandle.load(std::memory_order_acquire)) {
            LOGW("MatrixTask already running or still stopping");
            return;
        }
    }

    // Use static struct to avoid heap allocation
    static TaskParams params;
    params.menu = menu;
    params.imuService = imuService;
    params.imuManager = imuManager;
    params.matrixService = matrixService;
    params.matrixManager = matrixManager;
    params.bleService = bleService;
    params.wifiSensingService = wifiSensingService;
    params.csiService = csiService;

    if (!_stopAck) {
        _stopAck = xSemaphoreCreateBinary();
        if (!_stopAck) {
            LOGE("Failed to create MatrixTask stop ack semaphore");
            return;
        }
    }
    (void)xSemaphoreTake(_stopAck, 0);

    if (!_taskStack) {
        _taskStack = (StackType_t*)heap_caps_malloc(CONFIG::TASKS::STACK_MATRIX_TASK, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        _taskBuffer = (StaticTask_t*)heap_caps_malloc(sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }

    resetAutoRotationState();
    _shutdownEpilogueComplete.store(false, std::memory_order_release);
    _isRunning.store(true);

    if (_taskStack && _taskBuffer) {
        _taskHandle.store(xTaskCreateStaticPinnedToCore(
            taskLoop,
            "MatrixTask",
            CONFIG::TASKS::STACK_MATRIX_TASK,
            &params, // Pass static params struct pointer
            CONFIG::TASKS::PRIO_MATRIX_TASK,
            _taskStack,
            _taskBuffer,
            CONFIG::TASKS::CORE_MATRIX_TASK
        ), std::memory_order_release);
    }

    if (!_taskHandle.load(std::memory_order_acquire)) {
        LOGE("Failed to create MatrixTask");
        destroyTaskResources();
        return;
    }

    if (matrixManager) {
        // The terminal shutdown blackout intentionally clears renderer state,
        // while the manager still remembers the last top layer/hash. A supported
        // stop -> start lifecycle must therefore republish that unchanged layer
        // on the new worker's first update.
        matrixManager->invalidateCache();
    }
}

bool MatrixTask::stop() {
    const TaskHandle_t observedTaskHandle = _taskHandle.load(std::memory_order_acquire);

    if (observedTaskHandle && xTaskGetCurrentTaskHandle() == observedTaskHandle) {
        LOGW("MatrixTask::stop() called from worker context; requesting graceful exit only");
        _isRunning.store(false);
        return false;
    }

    const TickType_t shutdownWaitTicks = pdMS_TO_TICKS(TIMEOUT::TASK_SHUTDOWN_MS);
    if (!acquireLifecycleOwnership(shutdownWaitTicks)) {
        LOGE("Timed out waiting for another MatrixTask lifecycle owner");
        return false;
    }
    LifecycleOwnershipRelease releaseLifecycleOwnership(_lifecycleInProgress);

    const TaskHandle_t taskHandle = _taskHandle.load(std::memory_order_acquire);
    if (!taskHandle) {
        _isRunning.store(false);
        return _shutdownEpilogueComplete.load(std::memory_order_acquire);
    }

    _isRunning.store(false);

    (void)xTaskAbortDelay(taskHandle);

    // A retry after self-stop or an earlier timeout still gets a full bounded
    // wait. Using zero here can race the worker's blackout/ACK epilogue.
    if (!reapStoppedTask(shutdownWaitTicks)) {
        LOGE("MatrixTask did not complete its shutdown epilogue - skipping delete/free to avoid UAF");
        return false;
    }

    LOGI("MatrixTask stopped");
    return true;
}

bool MatrixTask::acquireLifecycleOwnership(TickType_t waitTicks) {
    const TickType_t pollTicks =
        TIMEOUT::TASK_SHUTDOWN_POLL_TICKS > 0 ? TIMEOUT::TASK_SHUTDOWN_POLL_TICKS : 1;
    TickType_t waited = 0;

    while (true) {
        bool expected = false;
        if (_lifecycleInProgress.compare_exchange_strong(
                expected,
                true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return true;
        }

        if (waited >= waitTicks) {
            return false;
        }

        const TickType_t remaining = waitTicks - waited;
        const TickType_t delayTicks = remaining < pollTicks ? remaining : pollTicks;
        vTaskDelay(delayTicks);
        waited += delayTicks;
    }
}

bool MatrixTask::reapStoppedTask(TickType_t waitTicks) {
    const TaskHandle_t taskHandle = _taskHandle.load(std::memory_order_acquire);
    if (!taskHandle) {
        destroyTaskResources();
        return _shutdownEpilogueComplete.load(std::memory_order_acquire);
    }

    if (_isRunning.load()) {
        return false;
    }

    if (!waitForTaskSuspended(taskHandle, _stopAck, waitTicks)) {
        return false;
    }
    if (!_shutdownEpilogueComplete.load(std::memory_order_acquire)) {
        return false;
    }

    vTaskDelete(taskHandle);
    destroyTaskResources();
    return true;
}

void MatrixTask::destroyTaskResources() {
    _taskHandle.store(nullptr, std::memory_order_release);
    _isRunning.store(false);
    resetAutoRotationState();

    if (_taskStack) {
        heap_caps_free(_taskStack);
        _taskStack = nullptr;
    }
    if (_taskBuffer) {
        heap_caps_free(_taskBuffer);
        _taskBuffer = nullptr;
    }
    if (_stopAck) {
        vSemaphoreDelete(_stopAck);
        _stopAck = nullptr;
    }
}

void MatrixTask::resetAutoRotationState() {
    _lastImuCheckMs = 0;
    _lastAutoRotateEnabled = false;
    _lastAppliedAutoRotation = 0xFF;
    _lastMatrixEffectsImuEnabled = false;
    _lastMatrixDataVizCsiEnabled = false;
    _lastDataVisualizationInputMs = 0;
}

void MatrixTask::taskLoop(void* param) {
    auto* params = static_cast<TaskParams*>(param);
    MatrixMenuService* menu = params->menu;
    ImuService* imuService = params->imuService;
    IMU::ImuManager* imuManager = params->imuManager;
    MatrixService* matrixService = params->matrixService;
    MATRIX_MANAGER::MatrixManagerService* matrixManager = params->matrixManager;
    BLE::BleService* bleService = params->bleService;
    WIFISENSING::WifiSensingService* wifiSensingService = params->wifiSensingService;
    WIFISENSING::CSI::CsiService* csiService = params->csiService;

    // Initial delay for power stabilization
    vTaskDelay(pdMS_TO_TICKS(UI::BOOT::TASK_STARTUP_DELAY_MS));

    // Register with TaskWatchdog to ensure the thread is monitored
    SYSTEM::TaskWatchdog::instance().registerCurrentTask();
    
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while(_isRunning.load()) {
        // Feed the watchdog to report that MatrixTask is still breathing
        SYSTEM::TaskWatchdog::instance().reset();
        // Menu update (if active) - check for time/content refresh
        if (menu) menu->update();
        
        // Auto-Rotation evaluation
        evaluateAutoRotation(imuService, imuManager, matrixService);
        evaluateEffectInput(imuService, imuManager, matrixService);
        evaluateDataVisualizationInput(bleService, wifiSensingService, csiService, matrixService);

        // Matrix Manager: resolve layers before rendering
        if (matrixManager) matrixManager->update();
        
        if (matrixService) {
            matrixService->loop();
        }
        
        // Monitoring
        LOG_STACK_PERIODIC(CONFIG::TASKS::STACK_MATRIX_TASK);

        TickType_t xFrequency = pdMS_TO_TICKS(CONFIG::TASKS::MATRIX_ACTIVE_INTERVAL_MS);
        if (matrixService && !matrixService->isActive()) {
            xFrequency = pdMS_TO_TICKS(CONFIG::TASKS::MATRIX_IDLE_INTERVAL_MS);
        }

        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }

    // MatrixTask is the sole renderer owner. Commit the terminal frame here,
    // before acknowledging shutdown, because deferred MatrixState commands can
    // no longer be consumed after this loop exits.
    bool blackoutSubmitted = false;
    if (matrixService) {
        matrixService->blackoutForShutdown();
        blackoutSubmitted = true;
    } else {
        LOGE("MatrixTask shutdown epilogue missing MatrixService");
    }

    SYSTEM::TaskWatchdog::instance().unregisterCurrentTask();
    _shutdownEpilogueComplete.store(blackoutSubmitted, std::memory_order_release);

    if (_stopAck) {
        xSemaphoreGive(_stopAck);
    }
    vTaskSuspend(nullptr);
}

void MatrixTask::evaluateAutoRotation(ImuService* imuService, IMU::ImuManager* imuManager, MatrixService* matrixService) {
    const bool autoRotate = RTC::getConfig().matrix.autoRotate;
    if (autoRotate != _lastAutoRotateEnabled) {
        LOGI("Auto-rotate %s", autoRotate ? "ON" : "OFF");
        if (imuManager) {
            imuManager->setConsumerActive(IMU::Consumer::AutoRotate, autoRotate);
        } else {
            LOGW("ImuManager missing - auto-rotate IMU consumer not updated");
        }
        // Reset the cached orientation state whenever the feature flips, so the
        // next enabled pass always reapplies the physical rotation immediately.
        _lastAutoRotateEnabled = autoRotate;
        _lastImuCheckMs = 0;
        _lastAppliedAutoRotation = 0xFF;
    }

    if (!autoRotate) return;
    const uint32_t now = millis();
    if (_lastImuCheckMs != 0 &&
        (now - _lastImuCheckMs) < UI::MATRIX::AUTO_ROTATE_INTERVAL_MS) {
        return;
    }
    _lastImuCheckMs = now;

    float ax, ay, az;
    if (!imuService || !imuService->getCachedAccel(ax, ay, az)) return;
    
    uint8_t newRot = RTC::getConfig().matrix.rotation;
    // Use dominant-axis approach: only react to the axis with strongest
    // gravity component. Prevents tilting (pitch) from triggering false
    // rotations on the weaker axis.
    const float absX = fabsf(ax);
    const float absY = fabsf(ay);

    if (absY >= absX && absY > UI::MATRIX::AUTO_ROTATE_THRESHOLD_G) {
        newRot = (ay > 0) ? 2 : 0;   // USB Bottom / USB Top
    } else if (absX > absY && absX > UI::MATRIX::AUTO_ROTATE_THRESHOLD_G) {
        newRot = (ax > 0) ? 1 : 3;   // USB Left / USB Right
    }
    
    if (newRot != _lastAppliedAutoRotation) {
        if (matrixService) {
            matrixService->setRotation(newRot);
        }
        _lastAppliedAutoRotation = newRot;
    }
}

void MatrixTask::evaluateEffectInput(ImuService* imuService, IMU::ImuManager* imuManager, MatrixService* matrixService) {
    const auto& matrixConfig = RTC::getConfig().matrix;
    const bool wantsImu =
        matrixConfig.effectEnabled &&
        matrixConfig.effectEngine == static_cast<uint8_t>(MATRIX_FX::EffectEngine::Native3D) &&
        matrixConfig.effectReactivityProvider == static_cast<uint8_t>(MATRIX_FX::ReactiveProvider::Imu);

    if (wantsImu != _lastMatrixEffectsImuEnabled) {
        LOGI("Matrix effect IMU input %s", wantsImu ? "ON" : "OFF");
        if (imuManager) {
            imuManager->setConsumerActive(IMU::Consumer::MatrixEffects, wantsImu);
        } else {
            LOGW("ImuManager missing - matrix effect IMU consumer not updated");
        }
        _lastMatrixEffectsImuEnabled = wantsImu;
    }

    if (!matrixService) {
        return;
    }

    MATRIX_FX::MatrixFxInput input;
    input.timestampMs = millis();

    if (wantsImu && imuService) {
        IMU::ImuSample sample;
        if (imuService->getCachedSample(sample)) {
            input.imuValid = true;
            input.ax = sample.ax;
            input.ay = sample.ay;
            input.az = sample.az;
            input.gx = sample.gx;
            input.gy = sample.gy;
            input.gz = sample.gz;
            const float accelMag = sqrtf(sample.ax * sample.ax + sample.ay * sample.ay + sample.az * sample.az);
            const float gyroMag = sqrtf(sample.gx * sample.gx + sample.gy * sample.gy + sample.gz * sample.gz);
            input.motionEnergy = fabsf(accelMag - 1.0f) + gyroMag * 0.001f;
        }
    }

    matrixService->setEffectInput(input);
}

void MatrixTask::evaluateDataVisualizationInput(BLE::BleService* bleService,
                                                WIFISENSING::WifiSensingService* wifiSensingService,
                                                WIFISENSING::CSI::CsiService* csiService,
                                                MatrixService* matrixService) {
    const auto& matrixConfig = RTC::getConfig().matrix;
    const MATRIX::MatrixDataVisualizationConfig vizConfig = buildDataVisualizationConfig(matrixConfig);
    const bool wantsDataVisualization =
        matrixConfig.backgroundMode == static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization) &&
        vizConfig.enabled;
    const bool wantsCsi =
        wantsDataVisualization &&
        vizConfig.source == static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi);

    if (wantsCsi != _lastMatrixDataVizCsiEnabled) {
        LOGI("Matrix data visualization CSI input %s", wantsCsi ? "ON" : "OFF");
        if (csiService) {
            csiService->setConsumerActive(WIFISENSING::CSI::CsiConsumer::MatrixVisualization, wantsCsi);
            _lastMatrixDataVizCsiEnabled =
                csiService->isConsumerActive(WIFISENSING::CSI::CsiConsumer::MatrixVisualization);
        } else {
            LOGW("CSI service missing - matrix data visualization consumer not updated");
            _lastMatrixDataVizCsiEnabled = wantsCsi;
        }
    }

    if (!matrixService || !wantsDataVisualization) {
        return;
    }

    const uint32_t now = millis();
    const uint32_t inputIntervalMs = dataVisualizationInputIntervalMs(vizConfig);
    if (_lastDataVisualizationInputMs != 0 &&
        (now - _lastDataVisualizationInputMs) < inputIntervalMs) {
        return;
    }
    _lastDataVisualizationInputMs = now;

    MATRIX::MatrixDataVisualizationInput input;
    input.timestampMs = now;
    input.reason = static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoSample);

    const auto source = static_cast<MATRIX::MatrixDataSource>(vizConfig.source);
    const auto metric = static_cast<MATRIX::MatrixDataMetric>(vizConfig.metric);

    switch (source) {
        case MATRIX::MatrixDataSource::Scd4x: {
            const SensorSnapshot snapshot = SENSORS::SensorState::getSnapshot();
            input.valid = hasScd4xReading(snapshot);
            input.stale = !input.valid ||
                !SENSORS::isSnapshotFresh(snapshot.timestamp_ms, now, SENSOR::SNAPSHOT_TIMEOUT_MS);
            input.reason = !input.valid
                ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoScd4xReading)
                : (input.stale
                    ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Stale)
                    : static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Ok));
            switch (metric) {
                case MATRIX::MatrixDataMetric::Temperature:
                    input.value = snapshot.temp;
                    break;
                case MATRIX::MatrixDataMetric::Humidity:
                    input.value = snapshot.humid;
                    break;
                case MATRIX::MatrixDataMetric::Co2:
                default:
                    input.value = static_cast<float>(snapshot.co2);
                    break;
            }
            break;
        }

        case MATRIX::MatrixDataSource::BleThermometer: {
            float temp = 0.0f;
            float humid = 0.0f;
            uint8_t batt = 0;
            int8_t rssi = -127;
            uint32_t lastSeen = 0;
            const char* selectedMac = vizConfig.deviceId[0] != '\0' ? vizConfig.deviceId : nullptr;
            bool ok = false;
            if (selectedMac) {
                ok = bleService && bleService->getCachedDeviceData(selectedMac, temp, humid, batt, rssi, lastSeen);
            } else if (bleService) {
                const char* cachedMac = nullptr;
                ok = bleService->getCachedDeviceDataAt(0, cachedMac, temp, humid, batt, rssi, lastSeen);
            }
            input.valid = ok;
            input.stale = !ok || lastSeen == 0 || (now - lastSeen) > UI::MATRIX::DATA_VISUALIZATION_BLE_STALE_MS;
            input.reason = !bleService
                ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoService)
                : (!ok
                    ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoBleDevice)
                    : (input.stale
                        ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Stale)
                        : static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Ok)));
            switch (metric) {
                case MATRIX::MatrixDataMetric::Humidity:
                    input.value = humid;
                    break;
                case MATRIX::MatrixDataMetric::Rssi:
                case MATRIX::MatrixDataMetric::SignalQuality:
                    input.value = metric == MATRIX::MatrixDataMetric::SignalQuality
                        ? rssiQuality(rssi)
                        : static_cast<float>(rssi);
                    break;
                case MATRIX::MatrixDataMetric::Temperature:
                default:
                    input.value = temp;
                    break;
            }
            input.secondary = static_cast<float>(batt);
            break;
        }

        case MATRIX::MatrixDataSource::WifiRssi: {
            if (wifiSensingService) {
                const WIFISENSING::RssiStats stats = wifiSensingService->getStats();
                const RssiBinsResult bins = fillRssiBins(wifiSensingService, input);
                input.valid = stats.sampleCount > 0 && bins.count > 0;
                const uint32_t sampleAgeMs = bins.newestTimestampMs == 0
                    ? UINT32_MAX
                    : (now - bins.newestTimestampMs);
                const uint32_t staleTimeoutMs =
                    wifiRssiStaleTimeoutMs(RTC::getConfig().wifiSensing.sampleIntervalMs);
                input.stale = !input.valid ||
                    !wifiSensingService->isActive() ||
                    sampleAgeMs > staleTimeoutMs;
                if (metric == MATRIX::MatrixDataMetric::SignalQuality) {
                    input.value = rssiQuality(stats.current);
                } else {
                    input.value = static_cast<float>(stats.current);
                }
                input.secondary = stats.variance;
            }

            if (!input.valid || input.stale) {
                const DirectRssiResult direct = readDirectRssi();
                if (direct.valid) {
                    input.valid = true;
                    input.stale = false;
                    input.value = metric == MATRIX::MatrixDataMetric::SignalQuality
                        ? rssiQuality(direct.rssi)
                        : static_cast<float>(direct.rssi);
                    input.secondary = 0.0f;
                    input.binCount = 0;
                    input.reason = static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Ok);
                } else {
                    input.reason = !direct.wifiActive
                        ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::WifiInactive)
                        : static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoSample);
                }
            } else {
                input.reason = static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Ok);
            }
            break;
        }

        case MATRIX::MatrixDataSource::WifiCsi: {
            if (csiService) {
                const WIFISENSING::CSI::CsiVisualizationSnapshot visualization =
                    csiService->getVisualizationSnapshot();
                input.valid = visualization.valid;
                input.stale = !input.valid ||
                    visualization.timestampMs == 0 ||
                    (now - visualization.timestampMs) > 5000;
                input.value = clampf(visualization.value, 0.0f, 100.0f);
                input.secondary = static_cast<float>(visualization.width);
                fillCsiBins(visualization, input);
                input.reason = !input.valid
                    ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoCsiPacket)
                    : (input.stale
                        ? static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Stale)
                        : static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::Ok));
            } else {
                input.reason = static_cast<uint8_t>(MATRIX::MatrixDataVisualizationReason::NoService);
            }
            break;
        }
    }

    matrixService->setDataVisualizationInput(input);
}

} // namespace MATRIX
