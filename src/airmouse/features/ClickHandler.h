#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "../managers/MouseActuator.h"
#include "../../macros/MacroService.h"
#include "../config/AirMouseConfigAdapter.h"

namespace AIRMOUSE {

/**
 * @brief Executes click actions triggered by a physical button.
 *
 * Uses RTC-configured mapping:
 * - single/double/triple click -> mouse click or macro script.
 * - only active when clickSource == BUTTON.
 */
class ClickHandler {
public:
    ClickHandler(MouseActuator* actuator, MACROS::MacroService* macroService, SemaphoreHandle_t hidMutex, AirMouseConfigAdapter* configAdapter);
    ~ClickHandler();

    void handleClick(RTC::ClickSource source, uint8_t clickCount);
    void handleButtonClick(uint8_t clickCount) { handleClick(RTC::ClickSource::BUTTON, clickCount); }
    void setMacroService(MACROS::MacroService* macroService) { _macroService = macroService; }

private:
    MouseActuator* _actuator;
    MACROS::MacroService* _macroService;
    SemaphoreHandle_t _hidMutex;
    AirMouseConfigAdapter* _configAdapter;
};

} // namespace AIRMOUSE
