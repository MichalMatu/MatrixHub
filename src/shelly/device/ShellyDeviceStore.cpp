/**
 * @file ShellyDeviceStore.cpp
 * @brief Shelly storage implementation
 */

#include "ShellyDeviceStore.h"
#include "../../system/logging/Logging.h"
#include "core/config/ConfigManager.h"

#undef LOG_TAG
#define LOG_TAG "ShellyStore"

namespace SHELLY {

ShellyDeviceStore::ShellyDeviceStore(FS& fs)
    : _repository(fs) {
  syncFromStore();
}

void ShellyDeviceStore::syncFromStore() {
  _data = CONFIG_STORE::copy();
}

uint8_t ShellyDeviceStore::load() {
  syncFromStore();
  LOGI("Loaded from config store: count=%d", _data.deviceCount);
  return _data.deviceCount;
}

bool ShellyDeviceStore::commit() {
  uint8_t enabledCount = 0;
  for (uint8_t i = 0; i < _data.deviceCount; i++) {
    if (_data.devices[i].enabled) {
      enabledCount++;
    }
  }

  const bool ok = CONFIG_STORE::update([&](RTC::ShellyData& shelly) {
    shelly = _data;
  });

  const bool summaryOk = RTC::updateConfigSection(&RTC::ConfigStore::shelly, [&](RTC::ShellySummaryData& shelly) {
    shelly.deviceCount = _data.deviceCount;
    shelly.enabledCount = enabledCount;
  });

  if (!ok || !summaryOk) {
    LOGW("Failed to commit Shelly snapshot");
    return false;
  }
  return true;
}

bool ShellyDeviceStore::save() {
  if (!CONFIG::save(_repository.getFs())) {
    LOGE("Failed to save to filesystem");
    return false;
  }

  LOGD("Saved %d devices to filesystem", _data.deviceCount);
  return true;
}

ShellyDevice* ShellyDeviceStore::findDevice(const char* id) {
  if (!id) return nullptr;
  for (uint8_t i = 0; i < _data.deviceCount; i++) {
    if (strcmp(_data.devices[i].id, id) == 0) {
      return &_data.devices[i];
    }
  }
  return nullptr;
}

bool ShellyDeviceStore::addDevice(const ShellyDevice& device) {
  if (_data.deviceCount >= RTC::kMaxShellyDevices) {
    LOGW("Device limit reached (%d)", RTC::kMaxShellyDevices);
    return false;
  }

  const RTC::ShellyData before = _data;
  _data.devices[_data.deviceCount] = device;
  _data.deviceCount++;
  if (!commit()) {
    _data = before;
    return false;
  }
  LOGI("Added device: %s (count=%d)", device.id, _data.deviceCount);
  return true;
}

bool ShellyDeviceStore::removeDevice(const char* id) {
  if (!id) return false;
  for (uint8_t i = 0; i < _data.deviceCount; i++) {
    if (strcmp(_data.devices[i].id, id) == 0) {
      const RTC::ShellyData before = _data;
      // Shift remaining elements left
      for (uint8_t j = i; j < _data.deviceCount - 1; j++) {
        _data.devices[j] = _data.devices[j + 1];
      }
      _data.deviceCount--;
      
      // Clear the vacated slot to prevent ghost data
      memset(&_data.devices[_data.deviceCount], 0, sizeof(ShellyDevice));
      if (!commit()) {
        _data = before;
        return false;
      }
      
      LOGI("Removed device: %s (count=%d)", id, _data.deviceCount);
      return true;
    }
  }
  return false;
}

bool ShellyDeviceStore::updateDevice(const char* id, const ShellyDevice& device) {
  if (!id) return false;
  ShellyDevice* existing = findDevice(id);
  if (!existing) {
    return false;
  }

  const RTC::ShellyData before = _data;

  // Preserve runtime state while replacing config fields.
  ShellyDevice runtime = *existing;

  *existing = device;

  existing->isOn = runtime.isOn;
  existing->isOnline = runtime.isOnline;
  existing->failedPolls = runtime.failedPolls;
  existing->zeroPowerCount = runtime.zeroPowerCount;
  existing->lastUpdate = runtime.lastUpdate;
  existing->power = runtime.power;
  existing->energy = runtime.energy;
  existing->voltage = runtime.voltage;
  existing->current = runtime.current;
  existing->temperature = runtime.temperature;
  existing->interval = runtime.interval;
  existing->pollBackoff = runtime.pollBackoff;
  existing->rssi = runtime.rssi;
  if (!commit()) {
    _data = before;
    return false;
  }

  LOGI("Updated device: %s", device.id);
  return true;
}

}  // namespace SHELLY
