#include "test_macro_logic_shared.h"

#include "esp_log.h"

#include <FS.h>
#include <cstdarg>
#include <esp_heap_caps.h>

#ifndef MALLOC_CAP_SPIRAM
#define MALLOC_CAP_SPIRAM 0x400
#endif

#ifndef MALLOC_CAP_INTERNAL
#define MALLOC_CAP_INTERNAL 0
#endif

#ifndef MALLOC_CAP_8BIT
#define MALLOC_CAP_8BIT 0
#endif

TaskHandle_t xTaskCreateStaticPinnedToCore(TaskFunction_t pxTaskCode,
                                           const char* const pcName,
                                           const uint32_t ulStackDepth,
                                           void* const pvParameters,
                                           UBaseType_t uxPriority,
                                           StackType_t* const puxStackBuffer,
                                           StaticTask_t* const pxTaskBuffer,
                                           const BaseType_t xCoreID) {
    (void)pxTaskCode;
    (void)pcName;
    (void)ulStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    (void)puxStackBuffer;
    (void)pxTaskBuffer;
    (void)xCoreID;
    return (TaskHandle_t)1;
}

TaskHandle_t xTaskCreateStatic(TaskFunction_t pxTaskCode,
                               const char* const pcName,
                               const uint32_t ulStackDepth,
                               void* const pvParameters,
                               UBaseType_t uxPriority,
                               StackType_t* const puxStackBuffer,
                               StaticTask_t* const pxTaskBuffer) {
    (void)pxTaskCode;
    (void)pcName;
    (void)ulStackDepth;
    (void)pvParameters;
    (void)uxPriority;
    (void)puxStackBuffer;
    (void)pxTaskBuffer;
    return (TaskHandle_t)1;
}

extern "C" {
size_t heap_caps_get_free_size(uint32_t caps) {
    (void)caps;
    return 1024 * 1024;
}
}

namespace LOG {
Settings Logging::_settings;

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("[%d] %s: ", level, tag);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

void Logging::logSection(const char* title) {
    (void)title;
}
}  // namespace LOG

namespace RTC {
static ConfigStore mockStore;

void resetMockStore() {
    mockStore = ConfigStore{};
    mockStore.macros.enabled = true;
    mockStore.macros.bootDelay = 0;
    mockStore.airMouse.movementEnabled = true;
    mockStore.airMouse.clickEnabled = true;
}

const ConfigStore& getConfig() {
    static bool initialized = false;
    if (!initialized) {
        resetMockStore();
        initialized = true;
    }
    return mockStore;
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    updater(mockStore);
    return true;
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(mockStore);
}
}  // namespace RTC

FS LittleFS;

namespace CONFIG {
bool save(FS& fs) {
    (void)fs;
    return true;
}
}  // namespace CONFIG

#include "../../src/macros/engine/MacroEngine.cpp"
#include "../../src/macros/engine/MacroEngineTask.cpp"
#include "../../src/macros/persistence/MacroRepository.cpp"
#include "../../src/macros/parsing/MacroParser.cpp"
#include "../../src/macros/MacroService.cpp"

KEYBOARD::KeyboardService mockKeyboard;
AIRMOUSE::AirMouseService mockAirMouse;

void configureEngine(MACROS::MacroEngine& engine, bool withAirMouse) {
    engine.setKeyboardService(&mockKeyboard);
    if (withAirMouse) {
        engine.setAirMouseService(&mockAirMouse);
    }
}

void setUp(void) {
    RTC::resetMockStore();
    mockKeyboard.resetStats();
    mockAirMouse.reset();
}

void tearDown(void) {}
