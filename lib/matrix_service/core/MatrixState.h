#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../types/MatrixTypes.h"
#include "MatrixCommand.h"

// Encapsulates synchronization and prioritization logic
class MatrixState {
public:
    MatrixState();
    
    // Thread-Safe Setters
    void requestIcon(IconType icon, uint32_t durationMs);
    void requestText(const char* text, uint32_t color, uint32_t durationMs);
    void requestClear(bool stopBackground);
    void requestSolid(uint32_t color);
    void requestEffect(uint8_t mode,
                       uint32_t speed,
                       uint32_t color,
                       uint32_t color2,
                       uint32_t color3,
                       uint32_t durationMs,
                       uint8_t engine = 0,
                       uint8_t reactivityProvider = 0,
                       uint8_t reactivityGain = 0);
    void requestDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config, uint32_t durationMs = 0);
    
    void setBrightness(uint8_t brightness);
    void setThermalBrightnessLimit(uint8_t limit);
    void setRotation(uint8_t rotation);
    void setNotificationColor(uint32_t color);
    
    // Main Loop Polling (Returns highest priority pending command)
    bool poll(MatrixCommand& outCommand);
    bool hasPendingCommands() const;
    
    // Background State Persistence
    struct BgEffect {
        bool active = false;
        uint8_t mode = 0;
        uint8_t engine = 0;
        uint32_t speed = UI::MATRIX::DEFAULT_EFFECT_SPEED;
        uint32_t color = 0;
        uint32_t color2 = 0;
        uint32_t color3 = 0;
        uint8_t reactivityProvider = 0;
        uint8_t reactivityGain = 0;
    };
    
    BgEffect getBackgroundEffect() const;
    void clearBackgroundEffect();

    struct BgDataVisualization {
        bool active = false;
        MATRIX::MatrixDataVisualizationConfig config{};
    };

    BgDataVisualization getBackgroundDataVisualization() const;
    void clearBackgroundDataVisualization();

    // Custom Icon Management
    void setCustomIcon(IconType type, const uint32_t* bitmap);
    bool getCustomIcon(IconType type, uint32_t* outBuffer) const;

private:
    mutable SemaphoreHandle_t _mutex = nullptr;
    mutable StaticSemaphore_t _mutexBuffer;

    // MatrixState is the cross-task mailbox for a multi-field command. Its
    // short critical sections must never continue after a timed-out lock,
    // otherwise the renderer can observe a torn command payload.
    
    // Hardware settings may be coalesced independently, but visible content is
    // a single latest-wins mailbox. Keeping one dirty bit per content type can
    // replay an older effect after a newer alarm/icon command on the next task
    // tick, leaving the layer manager and physical display out of sync.
    struct {
        uint8_t contentDirty      : 1;
        uint8_t brightnessDirty   : 1;
        uint8_t rotationDirty     : 1;
    } _flags = {0, 0, 0};

    MatrixCommand _pendingContent{};
    uint32_t _notificationColor = 0;
    
    uint8_t _pendingBrightness = UI::MATRIX::BRIGHTNESS_DEFAULT;
    uint8_t _lastPublishedBrightness = UI::MATRIX::BRIGHTNESS_DEFAULT;
    uint8_t _userTargetBrightness = UI::MATRIX::BRIGHTNESS_DEFAULT;
    uint8_t _thermalLimit = 255; // Default no limit
    uint8_t _pendingRotation = 0;
    
    // Background Effect Cache
    BgEffect _bgEffect;
    BgDataVisualization _bgDataVisualization;
    
    // Custom Icon Storage (Volatile Cache)
    // 3 icons * 256 bytes = 768 bytes
    // Index mapping: (int)type - 1. (1=INFO -> 0)
    // (A pixel value != 0 at index 0 indicates existence)
    uint32_t _customIcons[3][64];
    
    // Current logical mode (to determine override behavior)
    MatrixMode _currentMode = MatrixMode::OFF;

    MatrixCommand& replacePendingContent(CommandType type);
};
