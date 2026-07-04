/**
 * @file ShellyDeviceValidator.h
 * @brief Device validation logic extracted from ShellyDeviceManager
 */

#pragma once

#include "../ShellyTypes.h"

namespace SHELLY {

/**
 * @brief Validates Shelly device data.
 */
class ShellyDeviceValidator {
 public:
  /**
   * @brief Check if device has required fields (id, ip).
   */
  static bool isValid(const ShellyDevice& device);

  /**
   * @brief Validate IP address format and range (private IPv4).
   */
  static bool isValidIp(const char* ip);
};

}  // namespace SHELLY
