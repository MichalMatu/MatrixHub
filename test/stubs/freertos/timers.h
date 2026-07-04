#pragma once

#include "FreeRTOS.h"

typedef void (*TimerCallbackFunction_t)(TimerHandle_t);

namespace TEST_STUBS::FREERTOS {

struct TimerStub {
    const char* name = nullptr;
    TickType_t period = 0;
    UBaseType_t autoReload = pdFALSE;
    void* id = nullptr;
    TimerCallbackFunction_t callback = nullptr;
    bool active = false;
};

inline void resetTimerStubs() {}

} // namespace TEST_STUBS::FREERTOS

inline TimerHandle_t xTimerCreate(const char* pcTimerName,
                                  TickType_t xTimerPeriodInTicks,
                                  UBaseType_t uxAutoReload,
                                  void* pvTimerID,
                                  TimerCallbackFunction_t pxCallbackFunction) {
    auto* timer = new TEST_STUBS::FREERTOS::TimerStub();
    timer->name = pcTimerName;
    timer->period = xTimerPeriodInTicks;
    timer->autoReload = uxAutoReload;
    timer->id = pvTimerID;
    timer->callback = pxCallbackFunction;
    return static_cast<TimerHandle_t>(timer);
}

inline BaseType_t xTimerStart(TimerHandle_t xTimer, TickType_t xTicksToWait) {
    (void)xTicksToWait;
    if (!xTimer) return pdFALSE;
    static_cast<TEST_STUBS::FREERTOS::TimerStub*>(xTimer)->active = true;
    return pdPASS;
}

inline BaseType_t xTimerStop(TimerHandle_t xTimer, TickType_t xTicksToWait) {
    (void)xTicksToWait;
    if (!xTimer) return pdFALSE;
    static_cast<TEST_STUBS::FREERTOS::TimerStub*>(xTimer)->active = false;
    return pdPASS;
}

inline BaseType_t xTimerDelete(TimerHandle_t xTimer, TickType_t xTicksToWait) {
    (void)xTicksToWait;
    delete static_cast<TEST_STUBS::FREERTOS::TimerStub*>(xTimer);
    return pdPASS;
}

inline void* pvTimerGetTimerID(TimerHandle_t xTimer) {
    if (!xTimer) return nullptr;
    return static_cast<TEST_STUBS::FREERTOS::TimerStub*>(xTimer)->id;
}
