#include "BleStatusManager.h"

namespace BLE {

BleStatusManager::BleStatusManager() = default;

BleStatus BleStatusManager::getStatus() const {
  BleStatus status;
  status.scannerActive = _scannerActive.load();
  return status;
}

void BleStatusManager::setRunning(bool running) {
  _running.store(running);
}

void BleStatusManager::setScannerActive(bool active) {
  _scannerActive.store(active);
}

}  // namespace BLE
