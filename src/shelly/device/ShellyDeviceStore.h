/**
 * @file ShellyDeviceStore.h
 * @brief Shelly storage operations extracted from ShellyDeviceManager
 * 
 * Encapsulates Shelly config-store access and persistence operations.
 */

#pragma once

#include <FS.h>
#include "../ShellyTypes.h"
#include "../ShellyConfigStore.h"
#include "../repository/ShellyRepository.h"
#include "../../system/rtc/RtcConfig.h"

namespace SHELLY {

/**
 * @brief Manages Shelly storage backed by PSRAM config + RTC summary.
 * 
 * NOT thread-safe - caller must hold mutex.
 */
class ShellyDeviceStore {
 public:
  explicit ShellyDeviceStore(FS& fs);

  /**
   * @brief Load devices from config store (already hydrated during boot).
   * @return Device count.
   */
  uint8_t load();

  /**
   * @brief Save current state to filesystem.
   */
  bool save();

  /**
   * @brief Commit current local snapshot back to config store and RTC summary.
   */
  bool commit();

  /**
   * @brief Find device by ID in local Shelly snapshot.
   * @return Pointer to device or nullptr if not found.
   */
  ShellyDevice* findDevice(const char* id);

  /**
   * @brief Get device count.
   */
  uint8_t getCount() const { return _data.deviceCount; }

  /**
   * @brief Get device at index (no bounds check - caller must validate).
   */
  ShellyDevice& getDeviceAt(uint8_t index) { return _data.devices[index]; }

  /**
   * @brief Add device to local Shelly array.
   * @return true if added (space available), false if full.
   */
  bool addDevice(const ShellyDevice& device);

  /**
   * @brief Remove device by ID from local Shelly array.
   * @return true if removed.
   */
  bool removeDevice(const char* id);

  /**
   * @brief Update device in-place in local Shelly array.
   * @return true if found and updated.
   */
  bool updateDevice(const char* id, const ShellyDevice& device);

  /**
   * @brief Snapshot current Shelly state for rollback.
   */
  RTC::ShellyData snapshot() const { return _data; }

  /**
   * @brief Restore Shelly state from snapshot.
   */
  bool restore(const RTC::ShellyData& snapshot) {
    _data = snapshot;
    return commit();
  }

 private:
  void syncFromStore();
  ShellyRepository _repository;
  RTC::ShellyData _data{};
};

}  // namespace SHELLY
