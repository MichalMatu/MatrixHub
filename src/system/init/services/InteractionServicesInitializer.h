#pragma once

#include <memory>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../../utils/StaticService.h"

namespace KEYBOARD {
class KeyboardService;
}

namespace AIRMOUSE {
class AirMouseService;
}

namespace MACROS {
class MacroService;
}

namespace MATRIX_MANAGER {
class MatrixManagerService;
}

namespace SYSTEM {
class TaskWatchdog;
}

namespace USB_TERMINAL {
class UsbTerminalService;
}

class ImuService;
namespace IMU { class ImuManager; }

class InteractionServicesInitializer {
public:
    struct Deps {
        MATRIX_MANAGER::MatrixManagerService* matrixManager{nullptr};
        SemaphoreHandle_t fsMutex{nullptr};
        SYSTEM::TaskWatchdog* taskWatchdog{nullptr};
    };

    struct State {
        std::unique_ptr<KEYBOARD::KeyboardService>& keyboardService;
        std::unique_ptr<ImuService>& imuService;
        std::unique_ptr<IMU::ImuManager>& imuManager;
        std::unique_ptr<AIRMOUSE::AirMouseService>& airMouseService;
        std::unique_ptr<MACROS::MacroService>& macroService;
        SYSTEM::PsramStaticService<USB_TERMINAL::UsbTerminalService>& usbTerminalService;
    };

    static void initialize(const State& state, const Deps& deps);
};
