#ifndef BleLifecycleManager_h
#define BleLifecycleManager_h

#include <atomic>

/**
 * @brief Manages NimBLE stack lifecycle for scanner-only BLE.
 */
class BleLifecycleManager {
 public:
  BleLifecycleManager();
  ~BleLifecycleManager() = default;

  bool begin(const char* deviceName);
  void stop();
  bool isRunning() const { return _running.load(); }

 private:
  bool initNimBLE(const char* deviceName);
  void deinitNimBLE();

  std::atomic<bool> _running{false};
  std::atomic<bool> _stopping{false};
};

#endif
