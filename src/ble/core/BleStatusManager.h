#ifndef BleStatusManager_h
#define BleStatusManager_h

#include <atomic>
#include "../BleTypes.h"

namespace BLE {

/**
 * @brief Centralizes scanner-only BLE runtime status.
 */
class BleStatusManager {
 public:
  BleStatusManager();
  ~BleStatusManager() = default;

  BleStatus getStatus() const;
  void setRunning(bool running);
  void setScannerActive(bool active);

  bool isRunning() const { return _running.load(); }
  bool isScannerActive() const { return _scannerActive.load(); }

 private:
  std::atomic<bool> _running{false};
  std::atomic<bool> _scannerActive{false};
};

}  // namespace BLE

#endif
