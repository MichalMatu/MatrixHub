/**
 * @file ShellyDeviceValidator.cpp
 * @brief Device validation implementation
 */

#include "ShellyDeviceValidator.h"
#include "../validation/IpValidator.h"

namespace SHELLY {

bool ShellyDeviceValidator::isValid(const ShellyDevice& device) {
  return device.isValid();
}

bool ShellyDeviceValidator::isValidIp(const char* ip) {
  return IpValidator::isValidPrivateIp(ip);
}

}  // namespace SHELLY
