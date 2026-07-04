#include "ImuService.h"
#include <Wire.h>
#include <esp_heap_caps.h>
#include "../../utils/hardware/I2cUtils.h"

#include "../../config/Hardware.h"
#include "../../config/System.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "IMU"

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

} // namespace

// Interrupt Service Routine for IMU
static void IRAM_ATTR imuISR() {
    // In a real implementation, we'd set a flag or give a semaphore
    // For now, let's just assume polling or simple flag
    // _tapDetected = true; // Need to be careful with static/instance access in ISR
}

bool ImuService::begin() {
    LOGI("IMU begin requested");
    if (_initialized.load(std::memory_order_acquire)) {
        LOGI("IMU already initialized");
        return true;
    }
    if (_samplerTaskHandle != nullptr) {
        if (!_initialized.load(std::memory_order_acquire)) {
            (void)reapStoppedTask(0);
        }
        if (_samplerTaskHandle != nullptr) {
            LOGW("IMU sampler handle still present - refusing to start");
            return false;
        }
    }

    _stopRequested.store(false, std::memory_order_release);

    if (!_stopAck) {
        _stopAck = xSemaphoreCreateBinary();
        if (!_stopAck) {
            LOGE("Failed to create IMU stop ack semaphore");
            return false;
        }
    }
    // Clear any stale stop signal
    (void)xSemaphoreTake(_stopAck, 0);

    LOGI("Initializing QMI8658 on SDA=%d SCL=%d", IMU::SDA, IMU::SCL);
    
    if (CONFIG::IMU::ENABLE_I2C_RECOVERY) {
        pinMode(IMU::SDA, INPUT_PULLUP);
        pinMode(IMU::SCL, INPUT_PULLUP);
        if (digitalRead(IMU::SDA) == LOW) {
            LOGW("I2C bus appears stuck (SDA low). Attempting recovery...");
            UTILS::HARDWARE::I2cUtils::recoverBus(IMU::SDA, IMU::SCL);
        }
    }

    // Use Wire (I2C0) for IMU on pins 11/12
    // Wire.begin(IMU::SDA, IMU::SCL); // Handled by _qmi.begin()

    if (!_qmi.begin(Wire, IMU::ADDRESS, IMU::SDA, IMU::SCL)) {
        LOGE("Failed to find QMI8658 - check wiring!");
        return false;
    }
    
    // QMI8658 handles Fast Mode I2C; 400 kHz reduces the window in which a read
    // can be stretched by scheduler latency or bus stalls.
    Wire.setClock(CONFIG::IMU::I2C_CLOCK_HZ);
    Wire.setTimeOut(CONFIG::IMU::I2C_TIMEOUT_MS);
    
    LOGI("QMI8658 found! ID: 0x%x", _qmi.getChipID());

    // Configure Accelerometer
    _qmi.configAccelerometer(
        SensorQMI8658::ACC_RANGE_4G,
        // Keep sensor ODR modestly above the 100 Hz AirMouse loop to preserve headroom
        // without oversampling the I2C path for no visible cursor benefit.
        SensorQMI8658::ACC_ODR_250Hz,
        SensorQMI8658::LPF_MODE_0
    );
    _qmi.enableAccelerometer();
    
    // Configure Gyroscope (required for AirMouse cursor movement)
    _qmi.configGyroscope(
        SensorQMI8658::GYR_RANGE_1024DPS, 
        SensorQMI8658::GYR_ODR_224_2Hz,
        SensorQMI8658::LPF_MODE_3 
    );
    _qmi.enableGyroscope();

    // Cache scale factors once (ranges are fixed for this session).
    _accelScale = _qmi.getAccelerometerScales();
    _gyroScale = _qmi.getGyroscopeScales();
    
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }
    if (!_mutex) {
        LOGE("Failed to create IMU mutex");
        return false;
    }

    // This sampler performs local I2C reads and cache updates only.
    // It does not drive lwIP/TLS, raw sockets, or LittleFS, so its task stack
    // can live in PSRAM to free a small amount of internal DRAM.
    _samplerStack = (StackType_t*)heap_caps_malloc(
        CONFIG::TASKS::STACK_IMU_SAMPLER,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    _samplerTcb = (StaticTask_t*)heap_caps_malloc(
        sizeof(StaticTask_t),
        MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!_samplerStack || !_samplerTcb) {
        LOGE("Failed to allocate IMU sampler task buffers");
        if (_samplerStack) {
            heap_caps_free(_samplerStack);
            _samplerStack = nullptr;
        }
        if (_samplerTcb) {
            heap_caps_free(_samplerTcb);
            _samplerTcb = nullptr;
        }
        return false;
    }

    _samplerTaskHandle = xTaskCreateStaticPinnedToCore(
        samplerTask,
        "ImuSampler",
        CONFIG::TASKS::STACK_IMU_SAMPLER,
        this,
        CONFIG::TASKS::PRIO_IMU_SAMPLER,
        _samplerStack,
        _samplerTcb,
        CONFIG::TASKS::CORE_IMU_SAMPLER);
    if (!_samplerTaskHandle) {
        LOGE("Failed to create IMU sampler task");
        if (_samplerStack) {
            heap_caps_free(_samplerStack);
            _samplerStack = nullptr;
        }
        if (_samplerTcb) {
            heap_caps_free(_samplerTcb);
            _samplerTcb = nullptr;
        }
        return false;
    }

    LOGI("IMU sampler resources allocated");
    _initialized.store(true, std::memory_order_release);
    return true;
}

void ImuService::stop() {
    if (!_initialized.load(std::memory_order_acquire) && !_samplerTaskHandle) {
        destroyTaskResources();
        return;
    }

    LOGI("Stopping IMU sampler...");
    if (_samplerTaskHandle && xTaskGetCurrentTaskHandle() == _samplerTaskHandle) {
        LOGW("stop() called from within IMU sampler task! Requesting graceful exit.");
        _initialized.store(false, std::memory_order_release);
        _stopRequested.store(true, std::memory_order_release);
        return;
    }

    const bool wasRunning = _initialized.exchange(false, std::memory_order_acq_rel);
    _stopRequested.store(true, std::memory_order_release);
    if (_samplerTaskHandle) {
        (void)xTaskAbortDelay(_samplerTaskHandle);
    }

    const TickType_t waitTicks =
        wasRunning ? pdMS_TO_TICKS(TIMEOUT::TASK_SHUTDOWN_MS) : 0;
    if (!reapStoppedTask(waitTicks)) {
        // Keep task resources allocated on timeout instead of force-deleting the
        // sampler. This worker owns I2C transactions and a shared mutex; killing
        // it mid-read would make IMU stop/start races much harder to recover.
        if (wasRunning) {
            LOGW("IMU sampler did not stop cleanly - keeping resources allocated");
        }
        return;
    }

    LOGI("IMU stopped safely");
}

void ImuService::loop() {
    // Live polling moved to the dedicated sampler task on CORE_PRO.
}

bool ImuService::readAccelerometer(float& x, float& y, float& z) {
    if (!_initialized.load(std::memory_order_acquire) || !_mutex) return false;

    // [FIX] Timeout of 5ms is a safe compromise: fast enough to not cause
    // noticeable UI lag (scrolling), but long enough to acquire lock if AirMouse is busy.
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::IMU::ACCEL_READ_LOCK_TIMEOUT_MS));
    if (lock.isLocked()) {
        bool success = _qmi.getAccelerometer(x, y, z);
        if (success) {
            portENTER_CRITICAL(&_cacheMux);
            _cachedAx = x;
            _cachedAy = y;
            _cachedAz = z;
            _sampleTimestampMs = millis();
            _sampleValid = true;
            portEXIT_CRITICAL(&_cacheMux);
        }
        return success;
    }
    return false;
}

bool ImuService::getCachedAccel(float& x, float& y, float& z) const {
    portENTER_CRITICAL(&_cacheMux);
    const bool valid = _sampleValid;
    const uint32_t sampleTimestampMs = _sampleTimestampMs;
    const float cachedAx = _cachedAx;
    const float cachedAy = _cachedAy;
    const float cachedAz = _cachedAz;
    portEXIT_CRITICAL(&_cacheMux);

    if (!valid) return false;
    if (millis() - sampleTimestampMs > CONFIG::TASKS::IMU_SAMPLE_STALE_MS) return false;

    x = cachedAx;
    y = cachedAy;
    z = cachedAz;
    return true;
}

bool ImuService::getCachedSample(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) const {
    portENTER_CRITICAL(&_cacheMux);
    const bool valid = _sampleValid;
    const uint32_t sampleTimestampMs = _sampleTimestampMs;
    const float cachedAx = _cachedAx;
    const float cachedAy = _cachedAy;
    const float cachedAz = _cachedAz;
    const float cachedGx = _cachedGx;
    const float cachedGy = _cachedGy;
    const float cachedGz = _cachedGz;
    portEXIT_CRITICAL(&_cacheMux);

    if (!valid) return false;
    if (millis() - sampleTimestampMs > CONFIG::TASKS::IMU_SAMPLE_STALE_MS) return false;

    ax = cachedAx;
    ay = cachedAy;
    az = cachedAz;
    gx = cachedGx;
    gy = cachedGy;
    gz = cachedGz;
    return true;
}

bool ImuService::getCachedSample(IMU::ImuSample& sample) const {
    portENTER_CRITICAL(&_cacheMux);
    const bool valid = _sampleValid;
    const uint32_t sampleTimestampMs = _sampleTimestampMs;
    const float cachedAx = _cachedAx;
    const float cachedAy = _cachedAy;
    const float cachedAz = _cachedAz;
    const float cachedGx = _cachedGx;
    const float cachedGy = _cachedGy;
    const float cachedGz = _cachedGz;
    portEXIT_CRITICAL(&_cacheMux);

    sample.timestampMs = sampleTimestampMs;
    sample.valid = valid;
    sample.ax = cachedAx;
    sample.ay = cachedAy;
    sample.az = cachedAz;
    sample.gx = cachedGx;
    sample.gy = cachedGy;
    sample.gz = cachedGz;

    if (!valid) return false;
    if (millis() - sampleTimestampMs > CONFIG::TASKS::IMU_SAMPLE_STALE_MS) return false;
    return true;
}

void ImuService::updateCachedSample(float ax, float ay, float az, float gx, float gy, float gz) {
    portENTER_CRITICAL(&_cacheMux);
    _cachedAx = ax;
    _cachedAy = ay;
    _cachedAz = az;
    _cachedGx = gx;
    _cachedGy = gy;
    _cachedGz = gz;
    _sampleTimestampMs = millis();
    _sampleValid = true;
    portEXIT_CRITICAL(&_cacheMux);
}

bool ImuService::readDirect(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
    if (!_initialized.load(std::memory_order_acquire) || !_mutex) return false;

    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::IMU::BURST_READ_LOCK_TIMEOUT_MS));
    if (!lock.isLocked()) {
        return false;
    }

    uint8_t buffer[CONFIG::IMU::BURST_READ_BYTES];

    Wire.beginTransmission(IMU::ADDRESS);
    Wire.write(CONFIG::IMU::BURST_START_REGISTER);
    const int txStatus = Wire.endTransmission(false);

    size_t bytesRead = 0;

    if (txStatus == 0) {
        Wire.requestFrom(
            static_cast<int>(IMU::ADDRESS),
            static_cast<int>(CONFIG::IMU::BURST_READ_BYTES));
        bytesRead = Wire.readBytes(buffer, sizeof(buffer));
    }

    if (txStatus == 0 && bytesRead == sizeof(buffer)) {
        const int16_t rawAx = (int16_t)(buffer[1] << 8) | buffer[0];
        const int16_t rawAy = (int16_t)(buffer[3] << 8) | buffer[2];
        const int16_t rawAz = (int16_t)(buffer[5] << 8) | buffer[4];
        const int16_t rawGx = (int16_t)(buffer[7] << 8) | buffer[6];
        const int16_t rawGy = (int16_t)(buffer[9] << 8) | buffer[8];
        const int16_t rawGz = (int16_t)(buffer[11] << 8) | buffer[10];

        const float accelScale = (_accelScale != 0.0f) ? _accelScale : _qmi.getAccelerometerScales();
        const float gyroScale = (_gyroScale != 0.0f) ? _gyroScale : _qmi.getGyroscopeScales();

        ax = rawAx * accelScale;
        ay = rawAy * accelScale;
        az = rawAz * accelScale;
        gx = rawGx * gyroScale;
        gy = rawGy * gyroScale;
        gz = rawGz * gyroScale;

        updateCachedSample(ax, ay, az, gx, gy, gz);
        return true;
    }
    return false;
}

bool ImuService::readAll(float& ax, float& ay, float& az, float& gx, float& gy, float& gz) {
    if (getCachedSample(ax, ay, az, gx, gy, gz)) {
        return true;
    }
    return readDirect(ax, ay, az, gx, gy, gz);
}

void ImuService::samplerTask(void* param) {
    auto* instance = static_cast<ImuService*>(param);
    if (instance) {
        instance->runSamplerTask();
    }
    LOGI("IMU sampler suspending (safe for deletion)");
    if (instance) {
        instance->signalStopAck();
    }
    vTaskSuspend(nullptr);
}

void ImuService::runSamplerTask() {
    TickType_t lastWakeTime = xTaskGetTickCount();

    while (!_stopRequested.load(std::memory_order_acquire)) {
        float ax = 0.0f, ay = 0.0f, az = 0.0f, gx = 0.0f, gy = 0.0f, gz = 0.0f;
        readDirect(ax, ay, az, gx, gy, gz);
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(CONFIG::TASKS::IMU_SAMPLE_INTERVAL_MS));
    }
}

void ImuService::signalStopAck() {
    if (_stopAck) {
        (void)xSemaphoreGive(_stopAck);
    }
}

bool ImuService::reapStoppedTask(TickType_t waitTicks) {
    if (_samplerTaskHandle == nullptr) {
        destroyTaskResources();
        return true;
    }

    if (_initialized.load(std::memory_order_acquire) ||
        !_stopRequested.load(std::memory_order_acquire)) {
        return false;
    }

    if (!waitForTaskSuspended(_samplerTaskHandle, _stopAck, waitTicks)) {
        return false;
    }

    vTaskDelete(_samplerTaskHandle);
    destroyTaskResources();
    return true;
}

void ImuService::destroyTaskResources() {
    _samplerTaskHandle = nullptr;
    _stopRequested.store(false, std::memory_order_release);
    _sampleValid = false;

    // Keep the shared IMU mutex across stop/start cycles. The shutdown hardening
    // here is only about sampler task lifetime; tearing the mutex down on every
    // stop would reintroduce timing-sensitive cleanup races around task exit.
    if (_samplerStack) {
        heap_caps_free(_samplerStack);
        _samplerStack = nullptr;
    }
    if (_samplerTcb) {
        heap_caps_free(_samplerTcb);
        _samplerTcb = nullptr;
    }
}

bool ImuService::wasTapped() {
    // Simple placeholder
    return false; 
}
