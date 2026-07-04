#include "../../config/System.h"
#include "BleLifecycleManager.h"
#include "../../config/Network.h"

#include <NimBLEDevice.h>
#include <esp_bt.h>

static const char TAG[] PROGMEM = "BleLifecycle";

BleLifecycleManager::BleLifecycleManager() {}

bool BleLifecycleManager::begin(const char* deviceName) {
  if (_stopping.load()) {
    ESP_LOGW(TAG, "BLE lifecycle is stopping, cannot start yet");
    return false;
  }

  if (_running.load()) {
    ESP_LOGW(TAG, "BLE lifecycle already running");
    return true;
  }

  if (!initNimBLE(deviceName)) {
    ESP_LOGE(TAG, "Failed to initialize NimBLE");
    return false;
  }

  _running.store(true);
  ESP_LOGD(TAG, "BLE lifecycle started successfully");
  return true;
}

void BleLifecycleManager::stop() {
  if (!_running.load() || _stopping.load()) {
    return;
  }

  _stopping.store(true);
  ESP_LOGI(TAG, "Stopping BLE lifecycle...");

  deinitNimBLE();

  _running.store(false);
  _stopping.store(false);
  ESP_LOGI(TAG, "BLE lifecycle stopped");
}

bool BleLifecycleManager::initNimBLE(const char* deviceName) {
  ESP_LOGD(TAG, "Initializing NimBLE stack...");

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  // Do not mark BLE as running unless the underlying NimBLE stack really
  // finished initializing. NimBLEDevice::init() can fail on NVS/controller
  // bring-up and reports that via its bool return value.
  if (!NimBLEDevice::init(deviceName)) {
    ESP_LOGE(TAG, "NimBLEDevice::init() failed");
    return false;
  }

  NimBLEDevice::setPower(BLE::kTransmitPowerLevel);
  NimBLEDevice::setMTU(BLE::kMaxMtu);

  ESP_LOGD(TAG, "NimBLE stack initialized (MTU=%d, PowerLevel=%d)", BLE::kMaxMtu, BLE::kTransmitPowerLevel);
  return true;
}

void BleLifecycleManager::deinitNimBLE() {
  ESP_LOGI(TAG, "Deinitializing NimBLE stack...");

  // NimBLE-Arduino collapses the internal stop/deinit status to a bool:
  // true means the stack/controller shutdown completed successfully.
  // We intentionally pass clearAll=false so scanner objects survive a later
  // re-enable and can be reattached after NimBLE is initialized again.
  const bool ok = NimBLEDevice::deinit(false);
  if (ok) {
    ESP_LOGI(TAG, "NimBLE deinitialized successfully");
  } else {
    ESP_LOGE(TAG, "NimBLE deinit reported failure");
  }
}
