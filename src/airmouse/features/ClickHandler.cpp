#include "ClickHandler.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"
#include "../../config/System.h"

#undef LOG_TAG
#define LOG_TAG "AirMouseClick"

namespace AIRMOUSE {

ClickHandler::ClickHandler(MouseActuator* actuator, MACROS::MacroService* macroService, SemaphoreHandle_t hidMutex, AirMouseConfigAdapter* configAdapter)
    : _actuator(actuator), _macroService(macroService), _hidMutex(hidMutex), _configAdapter(configAdapter) {
}

ClickHandler::~ClickHandler() {
}

void ClickHandler::handleClick(RTC::ClickSource source, uint8_t clickCount) {
    if (!_actuator || !_hidMutex || !_configAdapter) return;
    
    if (!_configAdapter->isClickEnabled()) return;

    RTC::ClickSource clickSource;
    RTC::ClickAction singleAction;
    RTC::ClickAction doubleAction;
    RTC::ClickAction tripleAction;
    char singleScript[64] = {0};
    char doubleScript[64] = {0};
    char tripleScript[64] = {0};

    // Pull a consistent snapshot of click settings (thread-safe).
    if (!_configAdapter->getClickSettings(clickSource,
                                          singleAction, singleScript,
                                          doubleAction, doubleScript,
                                          tripleAction, tripleScript)) {
        LOGW("Click settings snapshot timed out");
        return;
    }

    if (clickSource != source) return;

    RTC::ClickAction action = RTC::ClickAction::NONE;
    char* scriptToRun = nullptr;

    if (clickCount == 1) {
        action = singleAction;
        scriptToRun = singleScript;
    } else if (clickCount == 2) {
        action = doubleAction;
        scriptToRun = doubleScript;
    } else if (clickCount >= 3) {
        action = tripleAction;
        scriptToRun = tripleScript;
    }

    if (action == RTC::ClickAction::NONE) return;

    // Note: We used to call _jiggler->notifyUserActivity() here.
    // However, jiggler notification now needs to be handled either by throwing an event
    // or through the MouseActuator itself (or the controller). We'll handle this in the Controller.
    // For now, ClickHandler just performs the click action.

    if (action == RTC::ClickAction::RUN_SCRIPT) {
        if (_macroService && scriptToRun && scriptToRun[0] != '\0') {
            LOGI("Click: source=%d count=%d action=RUN_SCRIPT script=%s", (int)source, clickCount, scriptToRun);
            _macroService->startScript(scriptToRun);
        } else {
            LOGW("Click %d: configured for script but missing service/filename", clickCount);
        }
        return;
    }

    // Mouse click actions must hold the HID mutex.
    SYSTEM::ScopeLock lock(_hidMutex, pdMS_TO_TICKS(CONFIG::AIR_MOUSE::HID::LOCK_TIMEOUT_MS));
    if (lock.isLocked()) {
        if (action == RTC::ClickAction::LEFT_CLICK)        _actuator->clickLeft();
        else if (action == RTC::ClickAction::RIGHT_CLICK)  _actuator->clickRight();
        else if (action == RTC::ClickAction::MIDDLE_CLICK) _actuator->click(MOUSE_MIDDLE);
        LOGI("Click: source=%d count=%d action=%d", (int)source, clickCount, (int)action);
    }
}

} // namespace AIRMOUSE
