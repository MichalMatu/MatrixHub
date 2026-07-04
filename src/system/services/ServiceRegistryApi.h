#pragma once

#include "../utils/StaticService.h"
#include "../../api/airmouse/AirMouseApiService.h"
#include "../../api/macros/MacroApiService.h"
#include "../../api/matrix/MatrixApiService.h"
#include "../../api/logs/LogFilesApiService.h"
#include "../../api/logs/LiveTailApiService.h"
#include "../../api/config/ConfigApiService.h"
#include "../../api/notifications/NotificationsApiService.h"
#include "../../api/wifisensing/WifiSensingApiService.h"
#include "../../api/system/SystemApiService.h"
#include "../../api/diagnostics/DiagnosticsApiService.h"
#include "../../api/alarms/AlarmsApiService.h"
#include "../../api/shelly/ShellyApiService.h"
#include "../../api/heartbeat/HeartbeatApiService.h"
#include "../../api/gpio/GpioApiService.h"
#include "../../api/ble/BleApiService.h"
#include "../../api/imu/ImuApiService.h"
#include "../../api/udp/UdpApiService.h"
#include "../../api/power/PowerApiService.h"
#include "../../api/keyboard/KeyboardApiService.h"
#include "../../api/compensation/CompensationApiService.h"
#include "../../api/usb_terminal/UsbTerminalApiService.h"

#include "api/filemanager/FileManagerApiService.h"
#include "filemanager/infrastructure/backends/service/StorageService.h"

/**
 * @brief Private struct holding all API PsramStaticService instances.
 *
 * This is an implementation detail of ServiceRegistry — only included
 * by ServiceRegistry internals and ApiServicesInitializer.cpp.
 * Keeps the heavyweight API includes out of the public header.
 */
struct ApiServices {
    SYSTEM::PsramStaticService<API::PowerApiService> powerApi;
    SYSTEM::PsramStaticService<API::LogFilesApiService> logFilesApi;
    SYSTEM::PsramStaticService<API::LiveTailApiService> liveTailApi;
    SYSTEM::PsramStaticService<API::ConfigApiService> configApi;
    SYSTEM::PsramStaticService<API::NotificationsApiService> notificationsApi;
    SYSTEM::PsramStaticService<API::WifiSensingApiService> wifiSensingApi;
    SYSTEM::PsramStaticService<API::SystemApiService> systemApi;
    SYSTEM::PsramStaticService<API::DiagnosticsApiService> diagnosticsApi;
    SYSTEM::PsramStaticService<API::AlarmsApiService> alarmsApi;
    SYSTEM::PsramStaticService<API::ShellyApiService> shellyApi;
    SYSTEM::PsramStaticService<API::BleApiService> bleApi;
    SYSTEM::PsramStaticService<API::ImuApiService> imuApi;
    SYSTEM::PsramStaticService<API::HeartbeatApiService> heartbeatApi;
    SYSTEM::PsramStaticService<API::GpioApiService> gpioApi;
    SYSTEM::PsramStaticService<API::UdpApiService> udpApi;
    SYSTEM::PsramStaticService<API::AirMouseApiService> airMouseApi;
    SYSTEM::PsramStaticService<API::KeyboardApiService> keyboardApi;
    SYSTEM::PsramStaticService<MACROS::MacroApiService> macroApi;
    SYSTEM::PsramStaticService<API::CompensationApiService> compensationApi;
    SYSTEM::PsramStaticService<API::MatrixApiService> matrixApi;
    SYSTEM::PsramStaticService<API::UsbTerminalApiService> usbTerminalApi;

    // File Manager
    SYSTEM::PsramStaticService<FILEMGR::StorageService> storageService;
    SYSTEM::PsramStaticService<API::FileManagerApiService> fileManager;

    /// Destroy all API services in reverse initialization order (LIFO)
    void destroyAll() {
        keyboardApi.destroy();
        udpApi.destroy();
        gpioApi.destroy();
        heartbeatApi.destroy();
        imuApi.destroy();
        bleApi.destroy();
        shellyApi.destroy();
        alarmsApi.destroy();
        diagnosticsApi.destroy();
        systemApi.destroy();
        wifiSensingApi.destroy();
        notificationsApi.destroy();
        configApi.destroy();
        compensationApi.destroy();
        liveTailApi.destroy();
        logFilesApi.destroy();
        powerApi.destroy();
        if (macroApi) macroApi.destroy();
        if (airMouseApi) airMouseApi.destroy();
        matrixApi.destroy();
        usbTerminalApi.destroy();

        // Destroy File Manager
        fileManager.destroy();
        storageService.destroy();
	}

};
