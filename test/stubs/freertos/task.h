#pragma once

#include "FreeRTOS.h"
#include <atomic>
#include <thread>

// Mock Task Functions
typedef void (*TaskFunction_t)( void * );

// eTaskState
typedef enum
{
	eRunning = 0,	/* A task is querying the state of itself, so must be running. */
	eReady,			/* The task being queried is in a read or pending ready list. */
	eBlocked,		/* The task being queried is in the Blocked state. */
	eSuspended,		/* The task being queried is in the Suspended state, or is in the Blocked state with an infinite time out. */
	eDeleted,		/* The task being queried has been deleted, but its TCB has not yet been freed. */
	eInvalid		/* Used as an 'invalid state' value. */
} eTaskState;

namespace TEST_STUBS::FREERTOS {
inline BaseType_t taskCreateResult = pdPASS;
inline TaskHandle_t createdTaskHandle = reinterpret_cast<TaskHandle_t>(1);
inline TaskHandle_t currentTaskHandle = reinterpret_cast<TaskHandle_t>(2);
inline BaseType_t schedulerState = 1;
inline std::atomic<eTaskState> taskState{eRunning};
inline TaskFunction_t lastTaskFunction = nullptr;
inline void* lastTaskParameter = nullptr;
inline const char* lastTaskName = nullptr;
inline uint32_t lastTaskStackDepth = 0;
inline UBaseType_t lastTaskPriority = 0;
inline BaseType_t lastTaskCore = tskNO_AFFINITY;
inline TickType_t tickCount = 0;
inline TaskHandle_t lastDeletedTask = nullptr;

inline void resetTaskCreateStub() {
    taskCreateResult = pdPASS;
    createdTaskHandle = reinterpret_cast<TaskHandle_t>(1);
    currentTaskHandle = reinterpret_cast<TaskHandle_t>(2);
    schedulerState = 1;
    taskState.store(eRunning, std::memory_order_relaxed);
    lastTaskFunction = nullptr;
    lastTaskParameter = nullptr;
    lastTaskName = nullptr;
    lastTaskStackDepth = 0;
    lastTaskPriority = 0;
    lastTaskCore = tskNO_AFFINITY;
    tickCount = 0;
    lastDeletedTask = nullptr;
}
}

#ifndef taskSCHEDULER_NOT_STARTED
#define taskSCHEDULER_NOT_STARTED 0
#endif

#ifndef taskSCHEDULER_RUNNING
#define taskSCHEDULER_RUNNING 1
#endif

inline BaseType_t xTaskCreate( TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const uint32_t usStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t * const pxCreatedTask ) {
    TEST_STUBS::FREERTOS::lastTaskFunction = pxTaskCode;
    TEST_STUBS::FREERTOS::lastTaskParameter = pvParameters;
    TEST_STUBS::FREERTOS::lastTaskName = pcName;
    TEST_STUBS::FREERTOS::lastTaskStackDepth = usStackDepth;
    TEST_STUBS::FREERTOS::lastTaskPriority = uxPriority;
    if (pxCreatedTask) {
        *pxCreatedTask = (TEST_STUBS::FREERTOS::taskCreateResult == pdPASS)
            ? TEST_STUBS::FREERTOS::createdTaskHandle
            : nullptr;
    }
    return TEST_STUBS::FREERTOS::taskCreateResult;
}

inline BaseType_t xTaskCreatePinnedToCore(
                            TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const uint32_t usStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            TaskHandle_t * const pxCreatedTask,
                            const BaseType_t xCoreID) {
    TEST_STUBS::FREERTOS::lastTaskCore = xCoreID;
    return xTaskCreate(pxTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pxCreatedTask);
}

#if !defined(TEST_STUBS_SKIP_XTASKCREATESTATICPINNEDTOCORE) && !defined(xTaskCreateStaticPinnedToCore)
inline TaskHandle_t xTaskCreateStaticPinnedToCore(
                            TaskFunction_t pxTaskCode,
                            const char * const pcName,
                            const uint32_t ulStackDepth,
                            void * const pvParameters,
                            UBaseType_t uxPriority,
                            StackType_t * const puxStackBuffer,
                            StaticTask_t * const pxTaskBuffer,
                            const BaseType_t xCoreID) {
    (void)puxStackBuffer;
    (void)pxTaskBuffer;
    TEST_STUBS::FREERTOS::lastTaskCore = xCoreID;
    TEST_STUBS::FREERTOS::lastTaskFunction = pxTaskCode;
    TEST_STUBS::FREERTOS::lastTaskParameter = pvParameters;
    TEST_STUBS::FREERTOS::lastTaskName = pcName;
    TEST_STUBS::FREERTOS::lastTaskStackDepth = ulStackDepth;
    TEST_STUBS::FREERTOS::lastTaskPriority = uxPriority;
    return (TEST_STUBS::FREERTOS::taskCreateResult == pdPASS)
        ? TEST_STUBS::FREERTOS::createdTaskHandle
        : nullptr;
}
#endif

#ifndef xTaskGetCurrentTaskHandle
inline TaskHandle_t xTaskGetCurrentTaskHandle() {
    return TEST_STUBS::FREERTOS::currentTaskHandle;
}
#endif

inline BaseType_t xTaskGetSchedulerState() {
    return TEST_STUBS::FREERTOS::schedulerState;
}

inline void vTaskDelete( TaskHandle_t xTaskToDelete ) {
    TEST_STUBS::FREERTOS::lastDeletedTask = xTaskToDelete;
    TEST_STUBS::FREERTOS::taskState.store(eDeleted, std::memory_order_release);
}

inline void vTaskDelay( const TickType_t xTicksToDelay ) {
    TEST_STUBS::FREERTOS::tickCount += xTicksToDelay;
    std::this_thread::yield();
}

inline BaseType_t xTaskDelayUntil(TickType_t* previousWakeTime, TickType_t timeIncrement) {
    if (previousWakeTime) {
        *previousWakeTime += timeIncrement;
        TEST_STUBS::FREERTOS::tickCount = *previousWakeTime;
    } else {
        TEST_STUBS::FREERTOS::tickCount += timeIncrement;
    }
    return pdTRUE;
}

#ifndef vTaskDelayUntil
#define vTaskDelayUntil(previousWakeTime, timeIncrement) ((void)xTaskDelayUntil((previousWakeTime), (timeIncrement)))
#endif

inline void vTaskSuspend( TaskHandle_t xTaskToSuspend ) {
    (void)xTaskToSuspend;
    TEST_STUBS::FREERTOS::taskState.store(eSuspended, std::memory_order_release);
}

inline void vTaskResume( TaskHandle_t xTaskToResume ) {}

inline BaseType_t xTaskAbortDelay( TaskHandle_t xTask ) { return pdPASS; }

inline TickType_t xTaskGetTickCount() {
    return TEST_STUBS::FREERTOS::tickCount;
}

inline UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask) {
    (void)xTask;
    return 1024;
}

inline const char* pcTaskGetName(TaskHandle_t xTask) {
    (void)xTask;
    return TEST_STUBS::FREERTOS::lastTaskName ? TEST_STUBS::FREERTOS::lastTaskName : "native-task";
}

inline eTaskState eTaskGetState( TaskHandle_t xTask ) {
    (void)xTask;
    return TEST_STUBS::FREERTOS::taskState.load(std::memory_order_acquire);
}
