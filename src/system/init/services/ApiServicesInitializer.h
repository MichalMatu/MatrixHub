#pragma once

#include <PsychicHttpServer.h>
#include <core/ESP32SvelteKit.h>

struct ApiServices;

namespace POWER {
class PowerManager;
class PowerSettingsService;
}

namespace SHELLY {
class ShellyService;
}

namespace WIFISENSING {
class WifiSensingSettings;
class WifiSensingService;
namespace CSI { class CsiService; }
}

namespace BLE {
class BleSettingsService;
class BleService;
}

namespace API {
class PushoverTestSender;
class TelegramTestSender;
class WebhookTestSender;
}

namespace SYSTEM { class HeartbeatSettingsService; }

namespace COMPENSATION { class CompensationSettingsService; }

namespace ALARMS {
class AlarmService;
class AlarmSettingsService;
}

namespace GPIO {
class GpioService;
}

namespace KEYBOARD {
class KeyboardService;
class KeyboardSettingsService;
}

namespace AIRMOUSE {
class AirMouseService;
}

namespace MACROS {
class MacroService;
}

namespace USB_TERMINAL {
class UsbTerminalService;
class UsbTerminalSettingsService;
}

namespace TELEGRAM {
class TelegramWorker;
}

namespace UDPPUSH {
class UdpPusher;
class UdpSettingsService;
}
namespace SYSTEM { class HeapMonitor; }

namespace IMU { class ImuRuntimeService; }

// Local composition bundle for the shared Phase-1 API boot path.
//
// The goal is purely structural: keep one explicit list of everything needed
// by ApiServicesInitializer instead of threading a 30+ argument function
// signature through multiple layers. If a shared API starts up with the wrong
// dependency, compare this bundle with the mapping in
// ServiceRegistryApiInitializationRuntime.cpp first.
struct ApiServicesInitializerDeps {
  PsychicHttpServer *server = nullptr;
  ESP32SvelteKit *framework = nullptr;
  POWER::PowerManager *powerManager = nullptr;
  POWER::PowerSettingsService *powerSettings = nullptr;
  SHELLY::ShellyService *shellyService = nullptr;
  WIFISENSING::WifiSensingSettings *wifiSensingSettings = nullptr;
  BLE::BleSettingsService *bleSettings = nullptr;
  COMPENSATION::CompensationSettingsService *compensationSettings = nullptr;
  ALARMS::AlarmService *alarmService = nullptr;
  ALARMS::AlarmSettingsService *alarmSettings = nullptr;
  GPIO::GpioService *gpioService = nullptr;
  WIFISENSING::CSI::CsiService *csiService = nullptr;
  WIFISENSING::WifiSensingService *wifiSensingService = nullptr;
  KEYBOARD::KeyboardService *keyboardService = nullptr;
  KEYBOARD::KeyboardSettingsService *keyboardSettingsService = nullptr;
  BLE::BleService *bleService = nullptr;
  MACROS::MacroService *macroService = nullptr;
  AIRMOUSE::AirMouseService *airMouseService = nullptr;
  USB_TERMINAL::UsbTerminalService *usbTerminalService = nullptr;
  TELEGRAM::TelegramWorker *telegramWorker = nullptr;
  API::TelegramTestSender *telegramTestSender = nullptr;
  API::WebhookTestSender *webhookTestSender = nullptr;
  API::PushoverTestSender *pushoverTestSender = nullptr;
  UDPPUSH::UdpPusher *udpPusher = nullptr;
  UDPPUSH::UdpSettingsService *udpSettings = nullptr;
  SYSTEM::HeartbeatSettingsService *heartbeatSettings = nullptr;
  USB_TERMINAL::UsbTerminalSettingsService *usbTerminalSettings = nullptr;
  SYSTEM::HeapMonitor *heapMonitor = nullptr;
  IMU::ImuRuntimeService *imuRuntimeService = nullptr;
  SemaphoreHandle_t fsMutex = nullptr;
};

/**
 * @brief Initializes all HTTP API endpoint services.
 *
 * Handles instantiation and begin() calls for:
 * - PowerApiService, LogFilesApiService, LiveTailApiService
 * - ConfigApiService, NotificationsApiService
 * - WifiSensingApiService, SystemApiService, AlarmsApiService
 * - ShellyApiService, BleApiService, HeartbeatApiService, UdpApiService
 *
 * Uses static BSS allocation (placement new) for memory management.
 *
 * The lifecycle-critical settings/services now live under ServiceRegistry
 * ownership, and API wrappers receive RTC-backed settings services via
 * injection instead of constructing their own persistent state.
 */
class ApiServicesInitializer {
public:
  // Keeps the full shared API boot order in one place. The helper functions in
  // the .cpp only partition this sequence for readability/debugging; they do
  // not change endpoint ownership or the init()/begin() order.
  static void initialize(ApiServices &api, const ApiServicesInitializerDeps& deps);
};
