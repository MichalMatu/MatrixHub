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
    // Keep this aligned with MatrixCommand::text so long messages are not truncated
    // before they even reach the renderer.
    static constexpr size_t kPendingTextCapacity = kMatrixTextCapacity;

    mutable SemaphoreHandle_t _mutex = nullptr;
    mutable StaticSemaphore_t _mutexBuffer;
    
    // State Flags (Prioritized Bitfield)
    struct {
        uint16_t clearDirty       : 1;
        uint16_t iconDirty        : 1;
        uint16_t textDirty        : 1;
        uint16_t colorDirty       : 1;
        uint16_t effectDirty      : 1;
        uint16_t dataVisualizationDirty : 1;
        uint16_t brightnessDirty  : 1;
        uint16_t rotationDirty    : 1;
        uint16_t stopBackgroundOnClear : 1;
    } _flags = {0,0,0,0,0,0,0,0,1};
    
    // Pending Data
    
    volatile IconType _pendingIcon = IconType::NONE;
    volatile uint32_t _pendingIconDuration = 0;
    
    char _pendingText[kPendingTextCapacity] = {0};
    volatile uint32_t _pendingTextColor = 0;
    volatile uint32_t _pendingTextDuration = 0;
    
    volatile uint32_t _notificationColor = 0;
    volatile uint32_t _activeSolidColor = 0;
    
    // Effect State
    volatile uint8_t _pendingEffectMode = 0;
    volatile uint8_t _pendingEffectEngine = 0;
    volatile uint32_t _pendingEffectSpeed = 0;
    volatile uint32_t _pendingEffectColor = 0;
    volatile uint32_t _pendingEffectColor2 = 0;
    volatile uint32_t _pendingEffectColor3 = 0;
    volatile uint32_t _pendingEffectDuration = 0;
    volatile uint8_t _pendingEffectReactivityProvider = 0;
    volatile uint8_t _pendingEffectReactivityGain = 0;
    MATRIX::MatrixDataVisualizationConfig _pendingDataVisualizationConfig{};
    volatile uint32_t _pendingDataVisualizationDuration = 0;
    
    volatile uint8_t _pendingBrightness = 0;
    volatile uint8_t _userTargetBrightness = 20; // Default fallback
    volatile uint8_t _thermalLimit = 255; // Default no limit
    volatile uint8_t _pendingRotation = 0;
    
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
};
