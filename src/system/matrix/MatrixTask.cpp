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
#undef LOG_TAG
#define LOG_TAG "MatrixTask"

namespace MATRIX {

TaskHandle_t MatrixTask::_taskHandle = nullptr;
StackType_t* MatrixTask::_taskStack = nullptr;
StaticTask_t* MatrixTask::_taskBuffer = nullptr;
SemaphoreHandle_t MatrixTask::_stopAck = nullptr;
std::atomic<bool> MatrixTask::_isRunning(false);
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

void fillRssiBins(WIFISENSING::WifiSensingService* wifiSensingService, MATRIX::MatrixDataVisualizationInput& input) {
    if (!wifiSensingService) {
        return;
    }
    WIFISENSING::RssiSample samples[MATRIX::kMatrixDataVizPixelCount];
    const uint16_t count = wifiSensingService->getSamples(samples, MATRIX::kMatrixDataVizPixelCount);
    if (count == 0) {
        return;
    }

    input.binCount = static_cast<uint8_t>(count > MATRIX::kMatrixDataVizPixelCount
        ? MATRIX::kMatrixDataVizPixelCount
        : count);
    for (uint8_t i = 0; i < input.binCount; ++i) {
        input.bins[i] = normalizedByte(rssiQuality(samples[i].rssi), 0.0f, 100.0f);
    }
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
    if (_taskHandle) {
        if (!_isRunning.load()) {
            (void)reapStoppedTask(0);
        }
        if (_taskHandle) {
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
    _isRunning.store(true);

    if (_taskStack && _taskBuffer) {
        _taskHandle = xTaskCreateStaticPinnedToCore(
            taskLoop,
            "MatrixTask",
            CONFIG::TASKS::STACK_MATRIX_TASK,
            &params, // Pass static params struct pointer
            CONFIG::TASKS::PRIO_MATRIX_TASK,
            _taskStack,
            _taskBuffer,
            CONFIG::TASKS::CORE_MATRIX_TASK
        );
    }

    if (!_taskHandle) {
        LOGE("Failed to create MatrixTask");
        destroyTaskResources();
    }
}

void MatrixTask::stop() {
    if (!_taskHandle && !_isRunning.load()) {
        return;
    }

    if (_taskHandle && xTaskGetCurrentTaskHandle() == _taskHandle) {
        LOGW("MatrixTask::stop() called from worker context; requesting graceful exit only");
        _isRunning.store(false);
        return;
    }

    const bool wasRunning = _isRunning.exchange(false);

    if (_taskHandle) {
        (void)xTaskAbortDelay(_taskHandle);
    }

    const TickType_t waitTicks = wasRunning ? pdMS_TO_TICKS(TIMEOUT::TASK_SHUTDOWN_MS) : 0;
    if (!reapStoppedTask(waitTicks)) {
        LOGE("MatrixTask did not suspend cleanly - skipping delete/free to avoid UAF");
        return;
    }

    LOGI("MatrixTask stopped");
}

bool MatrixTask::reapStoppedTask(TickType_t waitTicks) {
    if (!_taskHandle) {
        destroyTaskResources();
        return true;
    }

    if (_isRunning.load()) {
        return false;
    }

    if (!waitForTaskSuspended(_taskHandle, _stopAck, waitTicks)) {
        return false;
    }

    vTaskDelete(_taskHandle);
    destroyTaskResources();
    return true;
}

void MatrixTask::destroyTaskResources() {
    _taskHandle = nullptr;
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

    SYSTEM::TaskWatchdog::instance().unregisterCurrentTask();

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
        } else {
            LOGW("CSI service missing - matrix data visualization consumer not updated");
        }
        _lastMatrixDataVizCsiEnabled = wantsCsi;
    }

    if (!matrixService || !wantsDataVisualization) {
        return;
    }

    const uint32_t now = millis();
    if (_lastDataVisualizationInputMs != 0 &&
        (now - _lastDataVisualizationInputMs) < UI::MATRIX::DATA_VISUALIZATION_INPUT_INTERVAL_MS) {
        return;
    }
    _lastDataVisualizationInputMs = now;

    MATRIX::MatrixDataVisualizationInput input;
    input.timestampMs = now;

    const auto source = static_cast<MATRIX::MatrixDataSource>(vizConfig.source);
    const auto metric = static_cast<MATRIX::MatrixDataMetric>(vizConfig.metric);

    switch (source) {
        case MATRIX::MatrixDataSource::Scd4x: {
            const SensorSnapshot snapshot = SENSORS::SensorState::getSnapshot();
            input.valid = hasScd4xReading(snapshot);
            input.stale = !input.valid ||
                !SENSORS::isSnapshotFresh(snapshot.timestamp_ms, now, SENSOR::SNAPSHOT_TIMEOUT_MS);
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
                input.valid = stats.sampleCount > 0;
                input.stale = !input.valid;
                if (metric == MATRIX::MatrixDataMetric::SignalQuality) {
                    input.value = rssiQuality(stats.current);
                } else {
                    input.value = static_cast<float>(stats.current);
                }
                input.secondary = stats.variance;
                fillRssiBins(wifiSensingService, input);
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
            }
            break;
        }
    }

    matrixService->setDataVisualizationInput(input);
}

} // namespace MATRIX
