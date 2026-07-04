#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <PsychicHttpServer.h>
#include <core/ESP32SvelteKit.h>
#include "../memory/SystemAllocator.h"
#include "../utils/StaticService.h"
#include "../../matrix/MatrixSettingsService.h"
#include "../../usb_terminal/UsbTerminalService.h"

class MatrixService;
struct ApiServices;

namespace POWER {
class PowerManager;
class PowerSettingsService;
}

namespace KEYBOARD { class KeyboardService; }
namespace KEYBOARD { class KeyboardSettingsService; }
namespace AIRMOUSE { class AirMouseService; }
namespace AIRMOUSE { class AirMouseSettingsService; }

namespace SHELLY {
class ShellyService;
}

namespace WIFISENSING {
class WifiSensingSettings;
class WifiSensingService;
namespace CSI {
class CsiService;
}
} // namespace WIFISENSING

namespace BLE {
class BleService;
class BleSettingsService;
} // namespace BLE

namespace MATRIX {
class MatrixSettingsService;
class MatrixMenuService;
} // namespace MATRIX

namespace MATRIX_MANAGER {
class MatrixManagerService;
}

class ImuService;
namespace IMU {
class ImuManager;
class ImuRuntimeService;
}

namespace COMPENSATION {
class CompensationService;
class CompensationSettingsService;
} // namespace COMPENSATION

namespace MACROS {
class MacroApiService;
class MacroService;
class MacroSettingsService;
} // namespace MACROS

namespace USB_TERMINAL {
class UsbTerminalSettingsService;
class UsbTerminalService;
} // namespace USB_TERMINAL

namespace UDPPUSH {
class UdpPusher;
class UdpSettingsService;
}

namespace ALARMS {
class AlarmService;
class AlarmSettingsService;
}

namespace GPIO {
class GpioService;
}

namespace SENSORS {
class Scd4xSensorService;
} // namespace SENSORS

class NotificationSettingsService;

namespace SYSTEM {
class HeartbeatSettingsService;
}

namespace TELEGRAM {
class TelegramClient;
class MessageQueue;
class TelegramWorker;
struct TelegramCommandRuntimeState;
}

namespace NOTIFICATIONS {
class TelegramNotifier;
class WebhookTransportService;
class WebhookWorker;
class WebhookNotifier;
class PushoverTransportService;
class PushoverWorker;
class PushoverNotifier;
class NotificationWorker;
}

namespace API {
class SystemApiService;
class NotificationsApiService;
class PushoverTestSender;
class TelegramTestSender;
class WebhookTestSender;
}

/**
 * @brief Service container with RAII-managed service lifetimes
 * 
 * Uses unique_ptr for owned services to ensure proper cleanup on destruction.
 * Long-lived backend services are created and owned here so boot/shutdown
 * follows a single lifecycle model.
 * All notification services are owned and initialized here.
 *
 * API services (PsramStaticService) are held in an opaque ApiServices struct
 * to avoid pulling 20+ API headers into every translation unit.
 */
class ServiceRegistry {
public:
  ServiceRegistry();
  ~ServiceRegistry();

  void begin(PsychicHttpServer &server, ESP32SvelteKit &framework, SemaphoreHandle_t networkMutex, SemaphoreHandle_t notifMutex);
  
  bool isDying() const { return _isDying.load(std::memory_order_acquire); }

public:
  PsychicHttpServer *getServer() const {
    return _server;
  }

  ESP32SvelteKit *getFramework() const {
    return _framework;
  }

  POWER::PowerManager *getPowerManager() const {
    return _powerManager.get();
  }

  POWER::PowerSettingsService *getPowerSettingsService() const {
    return _powerSettings.get();
  }

  NotificationSettingsService *getNotificationSettingsService() const {
    return _notificationSettings.get();
  }
  
  SHELLY::ShellyService *getShellyService() const {
      return _shellyService.get();
  }
  
  BLE::BleService *getBleService() const {
      return _bleService.get();
  }
  
  BLE::BleSettingsService *getBleSettings() const {
      return _bleSettings.get();
  }

  WIFISENSING::CSI::CsiService *getCsiService() const {
      return _csiService.get();
  }

  WIFISENSING::WifiSensingService *getWifiSensingService() const {
      return _wifiSensingService.get();
  }

  WIFISENSING::WifiSensingSettings *getWifiSensingSettings() const {
      return _wifiSensingSettings.get();
  }

  MATRIX::MatrixMenuService* getMatrixMenu() const {
      return _matrixMenu.get();
  }

  MatrixService* getMatrixService() const {
      return _matrixService.get();
  }

  MATRIX_MANAGER::MatrixManagerService* getMatrixManager() const {
      return _matrixManager.get();
  }

  COMPENSATION::CompensationService* getCompensationService() const {
      return _compensationService.get();
  }

  COMPENSATION::CompensationSettingsService* getCompensationSettingsService() const {
      return _compensationSettings.get();
  }

  KEYBOARD::KeyboardService* getKeyboardService() const {
      return _keyboardService.get();
  }

  KEYBOARD::KeyboardSettingsService* getKeyboardSettingsService() const {
      return _keyboardSettings.get();
  }

  SENSORS::Scd4xSensorService* getScd4xSensorService() const {
      return _sensorService.get();
  }

  ImuService* getImuService() const {
      return _imuService.get();
  }

  IMU::ImuManager* getImuManager() const {
      return _imuManager.get();
  }

  IMU::ImuRuntimeService* getImuRuntimeService() const {
      return _imuRuntimeService.get();
  }

  ALARMS::AlarmService* getAlarmService() const {
      return _alarmService.get();
  }

  ALARMS::AlarmSettingsService* getAlarmSettingsService() const {
      return _alarmSettings.get();
  }

  GPIO::GpioService* getGpioService() const {
      return _gpioService.get();
  }
  
  AIRMOUSE::AirMouseService* getAirMouseService() const {
      return _airMouseService.get();
  }

  AIRMOUSE::AirMouseSettingsService* getAirMouseSettingsService() const {
      return _airMouseSettings.get();
  }

  MACROS::MacroService* getMacroService() const {
      return _macroService.get();
  }

  MACROS::MacroSettingsService* getMacroSettingsService() const {
      return _macroSettings.get();
  }

  USB_TERMINAL::UsbTerminalService* getUsbTerminalService() const {
      return _usbTerminalService.get();
  }

  USB_TERMINAL::UsbTerminalSettingsService* getUsbTerminalSettingsService() const {
      return _usbTerminalSettings.get();
  }

  UDPPUSH::UdpSettingsService* getUdpSettingsService() const {
      return _udpSettings.get();
  }

  UDPPUSH::UdpPusher* getUdpPusher() const {
      return _udpPusher.get();
  }

  NOTIFICATIONS::TelegramNotifier* getTelegramNotifier() const {
      return _notifications.telegramNotifier.get();
  }

  NOTIFICATIONS::WebhookNotifier* getWebhookNotifier() const {
      return _notifications.webhookNotifier.get();
  }

  NOTIFICATIONS::PushoverWorker* getPushoverWorker() const {
      return _notifications.pushoverWorker.get();
  }

  NOTIFICATIONS::PushoverNotifier* getPushoverNotifier() const {
      return _notifications.pushoverNotifier.get();
  }

  NOTIFICATIONS::NotificationWorker* getNotificationWorker() const {
      return _notifications.runtimeWorker.get();
  }

  API::TelegramTestSender* getTelegramTestSender() const {
      return _notifications.telegramTestSender.get();
  }

  API::PushoverTestSender* getPushoverTestSender() const {
      return _notifications.pushoverTestSender.get();
  }

  SemaphoreHandle_t getFsMutex() const {
      return _fsMutex;
  }

  API::WebhookTestSender* getWebhookTestSender() const {
      return _notifications.webhookTestSender.get();
  }

  API::NotificationsApiService* getNotificationsApiService() const;

  SYSTEM::HeartbeatSettingsService* getHeartbeatSettingsService() const {
      return _heartbeatSettings.get();
  }

private:
  struct NotificationServicesState {
    // Shared lifecycle flag for the whole notification runtime. "true" means the
    // worker owns the transports; "false" means every channel should unwind and
    // stop starting new network work as quickly as practical.
    std::atomic<bool> runtimeCancelToken{false};
    std::unique_ptr<TELEGRAM::TelegramClient> telegramClient;
    std::unique_ptr<TELEGRAM::MessageQueue> telegramQueue;
    // Telegram command parsing/reply building reuses one PSRAM scratch object
    // across inbound polls. Keeping ownership here removes the old hidden
    // worker-global state and makes lifecycle/shutdown explicit.
    SYSTEM::MEMORY::PsramUniquePtr<TELEGRAM::TelegramCommandRuntimeState> telegramCommandRuntime;
    std::unique_ptr<TELEGRAM::TelegramWorker> telegramWorker;
    std::unique_ptr<NOTIFICATIONS::TelegramNotifier> telegramNotifier;
    std::unique_ptr<NOTIFICATIONS::WebhookTransportService> webhookTransport;
    std::unique_ptr<NOTIFICATIONS::WebhookWorker> webhookWorker;
    std::unique_ptr<NOTIFICATIONS::WebhookNotifier> webhookNotifier;
    std::unique_ptr<NOTIFICATIONS::PushoverTransportService> pushoverTransport;
    std::unique_ptr<NOTIFICATIONS::PushoverWorker> pushoverWorker;
    std::unique_ptr<NOTIFICATIONS::PushoverNotifier> pushoverNotifier;
    std::unique_ptr<NOTIFICATIONS::NotificationWorker> runtimeWorker;
    std::unique_ptr<API::TelegramTestSender> telegramTestSender;
    std::unique_ptr<API::WebhookTestSender> webhookTestSender;
    std::unique_ptr<API::PushoverTestSender> pushoverTestSender;

    ~NotificationServicesState();
  };

  void bindRuntimeContext(PsychicHttpServer &server, ESP32SvelteKit &framework);
  void runInitializationSequence(SemaphoreHandle_t networkMutex, SemaphoreHandle_t notifMutex);
  void detachRuntimeCallbacks();
  void stopBackgroundWorkers();
  void destroyStaticServices();

  void initializeCoreServices();
  void initializeBusinessServices(SemaphoreHandle_t networkMutex);
  void initializeBleServices();
  void initializeMatrixServices();
  void initializeImuServices();
  void initializeInteractionServices();
  void initializeNotificationServices(SemaphoreHandle_t notifMutex);
  void initializeApiServices();
  void wireRuntimeCallbacks();

  SemaphoreHandle_t _fsMutex = nullptr;
  PsychicHttpServer *_server;
  ESP32SvelteKit *_framework;
  
  std::atomic<bool> _isDying{false};

  // Opaque API services container (defined in ServiceRegistryApi.h)
  std::unique_ptr<ApiServices> _api;

  // Step 4A cleanup: WiFi sensing settings now follow registry ownership just
  // like other long-lived config services instead of living in a separate
  // ad-hoc initialization path.
  std::unique_ptr<WIFISENSING::WifiSensingSettings> _wifiSensingSettings;
  // Step 4B cleanup: notification settings now follow the same registry-owned
  // lifecycle as their workers and runtime reconciliation callbacks.
  std::unique_ptr<NotificationSettingsService> _notificationSettings;
  // Step 4C cleanup: BLE settings now live under registry ownership and expose
  // detachable runtime hooks for whitelist refresh and live config updates.
  std::unique_ptr<BLE::BleSettingsService> _bleSettings;
  // Small follow-up cleanup: power settings now live here too so the power
  // API only exposes routes instead of owning persistent config state.
  std::unique_ptr<POWER::PowerSettingsService> _powerSettings;
  // Same cleanup for heartbeat settings: persistent config stays with the
  // registry while the API wrapper keeps only transport concerns.
  std::unique_ptr<SYSTEM::HeartbeatSettingsService> _heartbeatSettings;
  // Final cleanup: Shelly service now follows the same registry-owned
  // lifecycle as the rest of the backend services.
  std::unique_ptr<SHELLY::ShellyService> _shellyService;
  SYSTEM::PsramStaticService<MATRIX::MatrixSettingsService> _matrixSettings;
  
  // Owned Services
  std::unique_ptr<POWER::PowerManager> _powerManager;
  std::unique_ptr<WIFISENSING::CSI::CsiService> _csiService;
  std::unique_ptr<WIFISENSING::WifiSensingService> _wifiSensingService;
  std::unique_ptr<MatrixService> _matrixService;
  std::unique_ptr<MATRIX_MANAGER::MatrixManagerService> _matrixManager;
  std::unique_ptr<MATRIX::MatrixMenuService> _matrixMenu;
  std::unique_ptr<SENSORS::Scd4xSensorService> _sensorService;
  std::unique_ptr<ImuService> _imuService;
  std::unique_ptr<IMU::ImuManager> _imuManager;
  std::unique_ptr<IMU::ImuRuntimeService> _imuRuntimeService;
  std::unique_ptr<COMPENSATION::CompensationService> _compensationService;
  // Registry owns the RTC-backed compensation config so API stays transport-
  // only and the save/apply path follows the same lifecycle as the runtime.
  std::unique_ptr<COMPENSATION::CompensationSettingsService> _compensationSettings;
  std::unique_ptr<ALARMS::AlarmService> _alarmService;
  // Alarm rules updates are transactional and runtime-bound, so ownership
  // stays with the registry rather than a custom REST wrapper.
  std::unique_ptr<ALARMS::AlarmSettingsService> _alarmSettings;
  std::unique_ptr<GPIO::GpioService> _gpioService;
  std::unique_ptr<KEYBOARD::KeyboardService> _keyboardService;
  // Part 1 cleanup: keyboard config now follows registry-owned lifecycle
  // instead of being hidden inside KeyboardApiService.
  std::unique_ptr<KEYBOARD::KeyboardSettingsService> _keyboardSettings;
  std::unique_ptr<AIRMOUSE::AirMouseService> _airMouseService;
  // Part 2 cleanup: AirMouse config also lives under registry ownership so the
  // API layer no longer owns persistent state or runtime apply callbacks.
  std::unique_ptr<AIRMOUSE::AirMouseSettingsService> _airMouseSettings;
  std::unique_ptr<MACROS::MacroService> _macroService;
  // Macro config also follows registry ownership now; the API keeps validation
  // and script endpoints, but not the persistent RTC-backed settings state.
  std::unique_ptr<MACROS::MacroSettingsService> _macroSettings;
  SYSTEM::PsramStaticService<USB_TERMINAL::UsbTerminalService> _usbTerminalService;
  // Same cleanup for USB terminal: persistent config belongs to the registry,
  // while the API/websocket facade only exposes routes and session transport.
  std::unique_ptr<USB_TERMINAL::UsbTerminalSettingsService> _usbTerminalSettings;
  std::unique_ptr<BLE::BleService> _bleService;
  // UDP pusher now follows the same explicit lifecycle as other background
  // workers instead of hiding behind a singleton accessor.
  std::unique_ptr<UDPPUSH::UdpPusher> _udpPusher;
  // UDP pusher config is lightweight, but keeping it here removes one more
  // ad-hoc ownership path from the API layer.
  std::unique_ptr<UDPPUSH::UdpSettingsService> _udpSettings;

  NotificationServicesState _notifications;
  std::size_t _notificationSettingsHandlerId = 0;
  std::size_t _bleWhitelistSettingsHandlerId = 0;
};
