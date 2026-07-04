#include "KeyboardService.h"
#include "../system/logging/Logging.h"
#include "class/hid/hid_device.h" // For tud_hid_ready()
#include "../system/utils/ScopeLock.h"
#include "../config/System.h"

#undef LOG_TAG
#define LOG_TAG "Keyboard"

namespace KEYBOARD {

KeyboardService::KeyboardService() : _mutex(nullptr), _initialized(false) {
}

KeyboardService::~KeyboardService() {
    stop();
}

bool KeyboardService::begin() {
    if (_initialized) return true;
    
    // 1. Initialize Mutex first
    if (!_mutex) {
        _mutex = xSemaphoreCreateMutex();
    }
    
    if (!_mutex) {
        LOGE("Failed to create Keyboard Mutex");
        return false;
    }

    // 2. Register HID descriptors FIRST (before USB.begin() in InitSequence)
    _keyboard.begin();
    _consumer.begin();
    _system.begin();

    _initialized = true;
    
    LOGI("KeyboardService initialized (pending USB start)");
    return true;
}

void KeyboardService::stop() {
    if (_initialized) {
        _initialized = false; // Block new API requests
        if (_mutex) {
            // Lock safely before taking down endpoints
            SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
            _keyboard.end();
            // We intentionally DO NOT vSemaphoreDelete(_mutex) to avoid 
            // use-after-free FreeRTOS crashes if another task just acquired the reference.
        } else {
            _keyboard.end();
        }
        
        LOGI("KeyboardService stopped");
    }
}

void KeyboardService::type(const char* text) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _keyboard.print(text);
    } else {
        LOGW("Mutex timeout in type()");
    }
}

void KeyboardService::typeLn(const char* text) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _keyboard.println(text);
    } else {
        LOGW("Mutex timeout in typeLn()");
    }
}

void KeyboardService::press(uint8_t key) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _keyboard.write(key);
    } else {
        LOGW("Mutex timeout in press()");
    }
}

void KeyboardService::pressKey(uint8_t key) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _keyboard.press(key);
    } else {
        LOGW("Mutex timeout in pressKey()");
    }
}

void KeyboardService::releaseKey(uint8_t key) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _keyboard.release(key);
    } else {
        LOGW("Mutex timeout in releaseKey()");
    }
}

void KeyboardService::pressCombo(const uint8_t* keys, size_t count) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        for (size_t i = 0; i < count; i++) {
            _keyboard.press(keys[i]);
        }
        // macOS HID stack requires more hold time than Windows
        // to register system shortcuts (e.g. Ctrl+Cmd+Q, Cmd+Space).
        vTaskDelay(pdMS_TO_TICKS(CONFIG::KEYBOARD::MACOS_COMBO_HOLD_DELAY_MS));
        _keyboard.releaseAll();
    } else {
        LOGW("Mutex timeout in pressCombo()");
    }
}

void KeyboardService::pressConsumer(uint16_t usage) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _consumer.press(usage);
        _consumer.release();
    } else {
        LOGW("Mutex timeout in pressConsumer()");
    }
}

void KeyboardService::pressSystem(uint8_t usage) {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _system.press(usage);
        _system.release();
    } else {
        LOGW("Mutex timeout in pressSystem()");
    }
}

void KeyboardService::releaseAll() {
    if (!_initialized || !tud_hid_ready()) return;
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(CONFIG::KEYBOARD::MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
        _keyboard.releaseAll();
    } else {
        LOGW("Mutex timeout in releaseAll()");
    }
}

} // namespace KEYBOARD
