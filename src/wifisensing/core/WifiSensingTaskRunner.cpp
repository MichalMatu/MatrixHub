/**
 * @file WifiSensingTaskRunner.cpp
 * @brief Task runner implementation (extracted from WifiSensingService)
 */

#include "WifiSensingTaskRunner.h"
#include <esp_heap_caps.h>
#include "../../config/App.h"
#include "../../config/System.h"
#include "../../system/logging/Logging.h"
#include "../../system/Application.h"
#include "../../alarms/AlarmService.h"
#include "../analysis/RssiVarianceAnalyzer.h"
#include "../../system/watchdog/TaskWatchdog.h"
#include "../../system/utils/ScopeLock.h"
#include <ArduinoJson.h>
#include <cmath>

#undef LOG_TAG
#define LOG_TAG "WifiSensingTask"

static std::atomic<bool> s_taskSlotInUse{false};

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

namespace WIFISENSING {

WifiSensingTaskRunner::WifiSensingTaskRunner(RssiSampler& sampler, float varianceThreshold)
    : _sampler(sampler), _varianceThreshold(varianceThreshold) {
    _mutex = xSemaphoreCreateMutex();
}

WifiSensingTaskRunner::~WifiSensingTaskRunner() {
  while (_taskHandle != nullptr || _running.load(std::memory_order_acquire)) {
    stop();
    if (_taskHandle != nullptr) {
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  if (_mutex) {
    vSemaphoreDelete(_mutex);
  }
  freeTaskResources();
}

bool WifiSensingTaskRunner::allocateTaskResources() {
  if (_taskStack && _taskTcb) {
    return true;
  }

  // This runner only uses regular CPU-side WiFi/RSSI APIs and statistics over a
  // PSRAM-backed sample buffer. It does not drive raw sockets, TLS, LittleFS,
  // DMA, or ISR stack paths, so keeping the task stack in PSRAM is acceptable.
  // Verified on target hardware: leave this stack in PSRAM and do not move it
  // back to internal DRAM unless a concrete crash proves otherwise.
  // The current placement recovered about 4 KB of usable internal DRAM.
  if (!_taskStack) {
    _taskStack = (StackType_t*)heap_caps_malloc(
        SENSING_TASK_STACK_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  }

  // FreeRTOS task control blocks must remain in internal RAM.
  if (!_taskTcb) {
    _taskTcb = (StaticTask_t*)heap_caps_malloc(
        sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }

  if (_taskStack && _taskTcb) {
    return true;
  }

  LOGE("Failed to allocate WifiSensing task buffers");
  freeTaskResources();
  return false;
}

void WifiSensingTaskRunner::freeTaskResources() {
  if (_taskStack) {
    heap_caps_free(_taskStack);
    _taskStack = nullptr;
  }
  if (_taskTcb) {
    heap_caps_free(_taskTcb);
    _taskTcb = nullptr;
  }
  if (_taskHandle == nullptr && _stopAck) {
    vSemaphoreDelete(_stopAck);
    _stopAck = nullptr;
  }
}

bool WifiSensingTaskRunner::start(uint32_t sampleIntervalMs) {
  if (_taskHandle != nullptr) {
    if (_running.load(std::memory_order_acquire)) {
      LOGW("Task already running");
      return true;
    }
    if (!reapStoppedTask(0)) {
      LOGW("Task is still stopping");
      return false;
    }
  }

  if (_running.load(std::memory_order_acquire)) {
    LOGW("Task already running");
    return true;
  }

  if (s_taskSlotInUse.exchange(true)) {
    LOGE("Task slot already in use (only one WifiSensingTaskRunner supported)");
    return false;
  }

  if (sampleIntervalMs < LIMITS::WIFI_SENSING::MIN_INTERVAL_MS) {
    LOGW("Requested sample interval %ums below minimum, clamping to %ums",
         sampleIntervalMs,
         LIMITS::WIFI_SENSING::MIN_INTERVAL_MS);
    sampleIntervalMs = LIMITS::WIFI_SENSING::MIN_INTERVAL_MS;
  } else if (sampleIntervalMs > LIMITS::WIFI_SENSING::MAX_INTERVAL_MS) {
    LOGW("Requested sample interval %ums above maximum, clamping to %ums",
         sampleIntervalMs,
         LIMITS::WIFI_SENSING::MAX_INTERVAL_MS);
    sampleIntervalMs = LIMITS::WIFI_SENSING::MAX_INTERVAL_MS;
  }

  _sampleIntervalMs = sampleIntervalMs;
  _running.store(true, std::memory_order_release);
  _motionDetector.reset();
  _lastAlarmEvalMs = 0;

  if (!_stopAck) {
    _stopAck = xSemaphoreCreateBinary();
  }

  if (!_stopAck) {
    LOGE("Failed to create WifiSensing stop ack semaphore");
    _running.store(false, std::memory_order_release);
    s_taskSlotInUse.store(false);
    return false;
  }

  (void)xSemaphoreTake(_stopAck, 0);

  if (!allocateTaskResources()) {
    _running.store(false, std::memory_order_release);
    s_taskSlotInUse.store(false);
    return false;
  }

  _taskHandle = xTaskCreateStaticPinnedToCore(
      taskEntry,
      "wifi_sense",
      SENSING_TASK_STACK_SIZE,
      this,
      CONFIG::TASKS::PRIO_WIFI_SENSING,
      _taskStack,
      _taskTcb,
      CONFIG::TASKS::CORE_WIFI_SENSING
  );

  if (_taskHandle == nullptr) {
    LOGE("Failed to create task");
    _running.store(false, std::memory_order_release);
    s_taskSlotInUse.store(false);
    freeTaskResources();
    return false;
  }

  LOGI("Task started (PSRAM stack=%u, interval=%ums)", SENSING_TASK_STACK_SIZE, _sampleIntervalMs);
  return true;
}

bool WifiSensingTaskRunner::stop() {
  if (_taskHandle == nullptr && !_running.load(std::memory_order_acquire)) {
    return true;
  }

  if (_taskHandle != nullptr && xTaskGetCurrentTaskHandle() == _taskHandle) {
    LOGW("WifiSensingTaskRunner::stop() called from worker context; requesting graceful exit only");
    _running.store(false, std::memory_order_release);
    return false;
  }

  const bool wasRunning = _running.exchange(false, std::memory_order_acq_rel);

  if (_taskHandle) {
    // Wake the task out of vTaskDelayUntil() so stop/reconfigure doesn't block
    // for up to the full sampling interval.
    (void)xTaskAbortDelay(_taskHandle);
  }

  uint32_t maxWaitMs = _sampleIntervalMs * 5;
  if (maxWaitMs < 200) {
    maxWaitMs = 200;
  }
  const TickType_t waitTicks = wasRunning ? pdMS_TO_TICKS(maxWaitMs) : 0;
  if (!reapStoppedTask(waitTicks)) {
    LOGW("WifiSensing task did not suspend cleanly - keeping resources allocated");
    return false;
  }

  LOGI("Task stopped");
  return true;
}

bool WifiSensingTaskRunner::reapStoppedTask(TickType_t waitTicks) {
  if (_taskHandle == nullptr) {
    s_taskSlotInUse.store(false);
    freeTaskResources();
    return true;
  }

  if (_running.load(std::memory_order_acquire)) {
    return false;
  }

  if (!waitForTaskSuspended(_taskHandle, _stopAck, waitTicks)) {
    return false;
  }

  vTaskDelete(_taskHandle);
  _taskHandle = nullptr;
  s_taskSlotInUse.store(false);
  freeTaskResources();
  return true;
}

void WifiSensingTaskRunner::taskEntry(void* param) {
  auto* self = static_cast<WifiSensingTaskRunner*>(param);
  self->taskLoop();
}

void WifiSensingTaskRunner::taskLoop() {
  // Ensure clean state on task start
  _sampler.clear();

  // Initial stack measurement
  LOG_STACK_SIZE(SENSING_TASK_STACK_SIZE);

  // Register with Task Watchdog
  SYSTEM::TaskWatchdog::instance().registerCurrentTask();

  TickType_t xLastWakeTime = xTaskGetTickCount();
  auto feedWatchdog = []() {
    (void)SYSTEM::TaskWatchdog::instance().reset();
  };
  auto delayAndFeed = [&](uint32_t delayMs) {
    constexpr uint32_t kWatchdogStepMs = 1000;
    uint32_t remainingMs = delayMs;

    while (_running.load(std::memory_order_acquire) && remainingMs > 0) {
      const uint32_t stepMs = remainingMs > kWatchdogStepMs ? kWatchdogStepMs : remainingMs;
      vTaskDelay(pdMS_TO_TICKS(stepMs));
      feedWatchdog();
      remainingMs -= stepMs;
    }

    xLastWakeTime = xTaskGetTickCount();
  };

  while (_running.load(std::memory_order_acquire)) {
    feedWatchdog();

    // Step 0: Check WiFi mode and connection state
    wifi_mode_t mode = WiFi.getMode();
    
    // Case 1: Pure AP Mode
    if (mode == WIFI_AP) {
        if (WiFi.softAPgetStationNum() == 0) {
             // No clients to sense - suspend
             delayAndFeed(SENSOR::WIFI_SENSING::RETRY_DELAY_AP_MODE_MS);
             continue;
        }
        // If clients are connected, proceed to sampling
    } 
    // Case 2: STA or AP+STA Mode
    else if ((mode == WIFI_STA || mode == WIFI_AP_STA)) {
        bool hasConnection = WiFi.isConnected();
        bool hasApClients = (mode == WIFI_AP_STA) && (WiFi.softAPgetStationNum() > 0);

        if (!hasConnection && !hasApClients) {
            delayAndFeed(SENSOR::WIFI_SENSING::RETRY_DELAY_DISCONNECTED_MS);
            continue;
        }
    }
    // Case 3: OFF
    else {
        delayAndFeed(SENSOR::WIFI_SENSING::RETRY_DELAY_DISCONNECTED_MS);
        continue;
    }

    // Step 1: Take RSSI sample
    RssiSample sample = _sampler.takeSample();
    feedWatchdog();

    // Step 2 & 3: Stats and Callback (Throttled if interval is very small)
    bool shouldProcessStats = true;
    bool shouldInvokeCallback = true;

    // Optimization: If interval is < 200ms, only process stats/ws every N samples
    // to save CPU and reduce heat while still maintaining high-frequency sampling for alarms
    if (_sampleIntervalMs < 200) {
        static uint8_t throttleCounter = 0;
        throttleCounter++;
        // at 50ms: process every 4th sample (200ms effective)
        // at 20ms: process every 10th sample (200ms effective)
        uint8_t target = 200 / _sampleIntervalMs;
        if (throttleCounter < target) {
            shouldProcessStats = false;
            shouldInvokeCallback = false;
        } else {
            throttleCounter = 0;
        }
    }

    uint32_t now = millis();

    if (shouldProcessStats) {
        // Step 2: Calculate stats
        RssiStats stats = {};
        {
            SYSTEM::ScopeLock lock(_sampler.getMutex(), pdMS_TO_TICKS(50));
            if (lock.isLocked()) {
                LOG_PROFILE_START(rssiCalc);
                stats = RssiVarianceAnalyzer::calculateStats(
                    _sampler.getBufferUnsafe(),
                    _sampler.getCountUnsafe(),
                    _sampler.getHeadUnsafe(),
                    RSSI_BUFFER_SIZE
                );
                
                LOG_PROFILE_END_PERIODIC(rssiCalc, "RSSI stats calculation", TASK_MONITOR::INTERVAL_WIFI_RSSI_MS);
            }
        }
        feedWatchdog();

        // Step 3: Motion detection (only if enough samples)
        if (stats.sampleCount > 5) {
            if (_motionDetector.shouldPush(stats.variance, _varianceThreshold, now)) {
                // Log only on state change
                static bool lastLoggedState = false;
                if (_motionDetector.isMotionDetected() != lastLoggedState) {
                    LOGD("Motion %s (var=%.2f)", 
                         _motionDetector.isMotionDetected() ? "DETECTED" : "cleared", 
                         stats.variance);
                    lastLoggedState = _motionDetector.isMotionDetected();
                }
            }
        }

        // Step 3b: Invoke callbacks for streaming (multicast)
        SensingCallback localCallbacks[MAX_CALLBACKS];
        size_t localCallbackCount = 0;
        
        if (shouldInvokeCallback) {
            SYSTEM::ScopeLock lock(_mutex);
            if (lock.isLocked() && _callbackCount > 0) {
                LOG_PROFILE_START(wsCallback);
                for (size_t i = 0; i < _callbackCount; i++) {
                    localCallbacks[i] = _callbacks[i];
                }
                localCallbackCount = _callbackCount;
                LOG_PROFILE_END_PERIODIC(wsCallback, "Sensing WS callback copy", TASK_MONITOR::INTERVAL_WIFI_CALLBACK_MS);
            }
        }

        for (size_t i = 0; i < localCallbackCount; i++) {
            if (localCallbacks[i]) {
                feedWatchdog();
                localCallbacks[i](sample, stats, _motionDetector.isMotionDetected());
            }
            feedWatchdog();
        }

        // Step 4: Submit WiFi variance into the shared alarm snapshot. The
        // sensing loop stays focused on sampling and motion detection; rule
        // evaluation happens later from a single central execution point.
        if (now - _lastAlarmEvalMs >= ALARM::EVAL_INTERVAL_MS) {
            if (stats.sampleCount > 0 && !std::isnan(stats.variance)) {
                if (_alarmService) {
                    ALARMS::AlarmInputData input;
                    input.wifiVariance = stats.variance;
                    feedWatchdog();
                    _alarmService->submitInput(input);
                    feedWatchdog();
                }
            }
            _lastAlarmEvalMs = now;
        }
    }

    // Step 5: Stack monitoring (periodic)
    LOG_STACK_PERIODIC(SENSING_TASK_STACK_SIZE);
    feedWatchdog();

    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(_sampleIntervalMs));
    feedWatchdog();
    
  }

  // Unregister from Watchdog before deletion
  SYSTEM::TaskWatchdog::instance().unregisterCurrentTask();

  if (_stopAck) {
    xSemaphoreGive(_stopAck);
  }
  vTaskSuspend(nullptr);
}

}  // namespace WIFISENSING
