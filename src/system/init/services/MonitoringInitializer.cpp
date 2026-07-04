#include "MonitoringInitializer.h"

#include "../../../config/App.h"
#include "../../../airmouse/AirMouseService.h"
#include "../../button/ButtonHandler.h"
#include "../../health/SystemHealth.h"
#include "../../health/maintenance/HealthMaintenancePulse.h"
#include "../../health/heap/HeapMonitor.h"
#include "../../health/network/HttpServerHealthTracker.h"
#include "../../health/wifi/WifiHealthTracker.h"
#include "../../logging/Logging.h"
#include "../../rtc/RtcConfig.h"
#include "../../reset/FactoryReset.h"
#include "../../services/ServiceRegistry.h"
#include "../../thermal/ThermalMonitor.h"
#include "../../power/PowerManager.h"
#include "../../watchdog/TaskWatchdog.h"
#include "../../../sensors/SensorLoggingTask.h"
#include "../../../matrix/menu/MatrixMenuService.h"
#include <MatrixService.h>
#include <wifi/WiFiSettingsService.h>
#include <utility>

#undef LOG_TAG
#define LOG_TAG "MonitorInit"

namespace SYSTEM {
namespace {

MATRIX::WifiMenuAction wifiMenuActionFromMode(WiFiOperatingMode mode) {
    switch (mode) {
        case WiFiOperatingMode::Station:
            return MATRIX::WifiMenuAction::Station;
        case WiFiOperatingMode::AccessPoint:
            return MATRIX::WifiMenuAction::AccessPoint;
        case WiFiOperatingMode::Off:
        default:
            return MATRIX::WifiMenuAction::Off;
    }
}

WiFiOperatingMode wifiModeFromMenuAction(MATRIX::WifiMenuAction action) {
    switch (action) {
        case MATRIX::WifiMenuAction::Station:
            return WiFiOperatingMode::Station;
        case MATRIX::WifiMenuAction::AccessPoint:
            return WiFiOperatingMode::AccessPoint;
        case MATRIX::WifiMenuAction::Off:
        default:
            return WiFiOperatingMode::Off;
    }
}

void initializeButtonRouting(ServiceRegistry& services, ButtonHandler& buttonHandler) {
    auto* menu = services.getMatrixMenu();
    auto* powerManager = services.getPowerManager();
    if (menu) {
        if (auto* framework = services.getFramework()) {
            if (auto* wifiSettings = framework->getWiFiSettingsService()) {
                menu->setWifiModeActions(
                    [wifiSettings]() {
                        return wifiMenuActionFromMode(wifiSettings->getConfiguredMode());
                    },
                    [wifiSettings](MATRIX::WifiMenuAction action) {
                        return wifiSettings->setModeAndRestart(wifiModeFromMenuAction(action));
                    });
            }
        }
    }

    // Phase 7 is the single source of truth for button side effects. If button
    // behavior regresses, debug the bindings here first before inspecting
    // ButtonHandler's gesture engine.
    buttonHandler.begin();
    ButtonHandler::Bindings bindings;
    // ButtonHandler stays a pure gesture/input component. Phase 7 owns the
    // product wiring so future regressions can be debugged from one place.
    bindings.onActivity = [powerManager]() {
        if (powerManager) {
            powerManager->notifyActivity("button");
        }
    };
    bindings.isMenuActive = [menu]() {
        return menu && menu->isActive();
    };
    bindings.isMenuEnabled = [menu]() {
        return menu && menu->isEnabled();
    };
    bindings.onMenuNext = [menu]() {
        if (menu) {
            menu->nextScreen();
        }
    };
    // Enter-menu wiring intentionally keeps the existing side effect of
    // forcing one fresh sensor/log snapshot when the user opens the menu.
    bindings.onMenuEnter = [menu]() {
        if (menu) {
            menu->enterMenu();
            SensorLoggingTask::sendCommand(CMD_FORCE_READ_AND_LOG);
        }
    };
    bindings.onMenuSelect = [menu]() {
        if (menu) {
            menu->selectCurrent();
        }
    };
    bindings.onFactoryReset = [&services]() {
        SYSTEM::performFactoryReset(services);
    };

    // Visual feedback for the staged factory-reset gesture so a user holding
    // the button (or a physically stuck button) sees what is about to happen
    // and gets a chance to abort. Colors are chosen to be obvious even at low
    // matrix brightness: amber warning, red armed, green cancelled.
    auto* matrix = services.getMatrixService();
    if (matrix) {
        bindings.onResetWarning = [matrix]() {
            matrix->showText("RESET?", 0xFFA000, FACTORY::LONG_PRESS_MS - FACTORY::RESET_WARNING_MS);
        };
        bindings.onResetArmed = [matrix]() {
            matrix->showText("RELEASE +2x", 0xFF0000, FACTORY::RESET_CONFIRM_WINDOW_MS);
        };
        bindings.onResetCancelled = [matrix]() {
            matrix->showText("CANCEL", 0x00C800, 1500);
        };
    }

    if (auto* airMouse = services.getAirMouseService()) {
        airMouse->setMacroService(services.getMacroService());
        bindings.onMultiClick = [airMouse](uint8_t clickCount) {
            airMouse->handleButtonClick(clickCount);
        };

        // AirMouse owns the timing policy, but Phase 7 owns the physical button
        // instance. The sink keeps that relationship explicit without reviving a
        // global ButtonHandler singleton.
        airMouse->setPhysicalButtonDoubleClickWindowSink([&buttonHandler](uint32_t ms) {
            buttonHandler.setDoubleClickWindowMs(ms);
        });

        // Seed the handler once during boot so the physical button and
        // AirMouse config start in sync even before the next applySettings().
        const auto& airMouseConfig = RTC::getConfig().airMouse;
        buttonHandler.setDoubleClickWindowMs(airMouseConfig.doubleClickWindowMs);
    }

    buttonHandler.setBindings(std::move(bindings));
}

void initializeThermalMonitor(ServiceRegistry& services) {
    auto& thermal = ThermalMonitor::instance();
    thermal.setBleService(services.getBleService());
    thermal.setPowerManager(services.getPowerManager());
    thermal.setMatrixService(services.getMatrixService());
    if (!thermal.begin()) {
        // Thermal monitoring is valuable, but a failure here should not block
        // the entire product from booting into its primary network/dashboard path.
        LOGW("[Phase7] Failed to start thermal monitor - continuing without it");
    }
}

}  // namespace

void MonitoringInitializer::initialize(ServiceRegistry& services, ButtonHandler& buttonHandler) {
    initializeButtonRouting(services, buttonHandler);
    HeapMonitor::instance().setPowerManager(services.getPowerManager());
    HeapMonitor::instance().begin();
    HEALTH::HttpServerHealthTracker::reset();
    auto& watchdog = TaskWatchdog::instance();
    if (watchdog.isInitialized() && !watchdog.registerCurrentTask()) {
        LOGW("[Phase7] Failed to register main task with watchdog - continuing without main-loop supervision");
    }
    SystemHealth::begin();
    if (auto* framework = services.getFramework()) {
        if (auto* wifiSettings = framework->getWiFiSettingsService()) {
            HEALTH::WifiHealthTracker::setRecoveryRequester(
                [wifiSettings](const char* reason) {
                    return wifiSettings->requestRecovery(reason);
                });
        }
    }
    HealthMaintenancePulse::begin();
    initializeThermalMonitor(services);

    LOGI("[Phase7] Monitoring initialized");
}

}  // namespace SYSTEM
