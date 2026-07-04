#pragma once

#include <stdint.h>

// Mock FreeRTOS basic types for native tests
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t TickType_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef void* TimerHandle_t;
typedef uint32_t StackType_t;
typedef void* StaticTask_t;
typedef void (*TaskFunction_t)(void*);

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdFAIL 0

#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1

#define tskNO_AFFINITY 0x7FFFFFFF

typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0

#ifndef portENTER_CRITICAL
#define portENTER_CRITICAL(mux) ((void)(mux))
#endif
#ifndef portEXIT_CRITICAL
#define portEXIT_CRITICAL(mux) ((void)(mux))
#endif
#ifndef portENTER_CRITICAL_ISR
#define portENTER_CRITICAL_ISR(mux) ((void)(mux))
#endif
#ifndef portEXIT_CRITICAL_ISR
#define portEXIT_CRITICAL_ISR(mux) ((void)(mux))
#endif
#ifndef portYIELD_FROM_ISR
#define portYIELD_FROM_ISR() ((void)0)
#endif

#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)
#endif

inline bool xPortInIsrContext() {
    return false;
}
