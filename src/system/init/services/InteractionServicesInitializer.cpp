#include "InteractionServicesInitializer.h"

#include "../../../airmouse/AirMouseService.h"
#include "../../../keyboard/KeyboardService.h"
#include "../../../macros/MacroService.h"
#include "../../../usb_terminal/UsbTerminalService.h"
#include "../../logging/Logging.h"
#include "../../rtc/RtcConfig.h"
#include "../../usb/UsbBootPolicy.h"
#include "../../watchdog/TaskWatchdog.h"

void InteractionServicesInitializer::initialize(const State& state, const Deps& deps) {
    const RTC::ConfigStore& config = RTC::getConfig();
    const bool shouldStartKeyboard = SYSTEM::UsbBootPolicy::shouldStartKeyboardServiceOnBoot(config);
    const bool shouldStartAirMouse = SYSTEM::UsbBootPolicy::shouldStartAirMouseServiceOnBoot(config);
    const bool shouldStartUsbTerminal = SYSTEM::UsbBootPolicy::shouldStartUsbTerminalServiceOnBoot(config);

    if (shouldStartKeyboard) {
        state.keyboardService = std::make_unique<KEYBOARD::KeyboardService>();
        if (!state.keyboardService->begin()) {
            LOGE("KeyboardService failed to initialize");
            state.keyboardService.reset();
        }
    } else {
        LOGI("Skipping KeyboardService init - no persisted USB feature needs it");
    }

    if (shouldStartAirMouse) {
        if (!state.imuService) {
            LOGE("ImuService missing - AirMouse will run without IMU");
        }

        state.airMouseService = std::make_unique<AIRMOUSE::AirMouseService>(state.imuService.get());
        state.airMouseService->setImuManager(state.imuManager.get());

        const auto& airMouseConfig = config.airMouse;
        LOGI("AirMouse Config Check: movement=%d click=%d source=%d",
             airMouseConfig.movementEnabled,
             airMouseConfig.clickEnabled,
             static_cast<int>(airMouseConfig.clickSource));

        if (!state.keyboardService) {
            LOGE("KeyboardService missing - cannot initialize AirMouseService");
            state.airMouseService.reset();
        } else if (!state.airMouseService->begin(state.keyboardService.get(), deps.taskWatchdog)) {
            LOGE("AirMouseService failed to initialize");
            state.airMouseService.reset();
        }
    } else {
        LOGI("Skipping AirMouseService init - AirMouse USB features are disabled");
    }

    state.macroService = std::make_unique<MACROS::MacroService>(
        state.keyboardService.get(),
        state.airMouseService.get(),
        deps.matrixManager,
        deps.fsMutex);
    state.macroService->begin();

    if (shouldStartUsbTerminal) {
        if (!state.keyboardService) {
            LOGE("KeyboardService missing - cannot initialize UsbTerminalService");
        } else {
            state.usbTerminalService.init(state.keyboardService.get());
            state.usbTerminalService->begin();
        }
    } else {
        LOGI("Skipping UsbTerminalService init - USB terminal is disabled");
    }
}
