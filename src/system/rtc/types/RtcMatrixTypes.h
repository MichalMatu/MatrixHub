/**
 * @file RtcMatrixTypes.h
 * @brief RTC-persistent types for Matrix LED configuration
 */

#pragma once

#include "../RtcDefaultValues.h"
#include "../../../matrix/MatrixDataVisualizationTypes.h"
#include <cstdint>

namespace RTC {

/**
 * @brief Display mode for alarm notifications on Matrix LED
 */
enum class MatrixAlarmMode : uint8_t {
    SOLID_COLOR = 0,   // Fill screen with severity color (classic, power-hungry)
    ICON = 1,          // Show severity icon (⚠ ❌ ✓) - recommended
    SCROLL_TEXT = 2    // Scroll alarm name as text
};

/**
 * @brief Menu screen selection for button-controlled display
 */
enum class MatrixMenuScreen : uint8_t {
    NONE = 0,      // Menu closed - normal operation
    TIME = 1,      // Date and time display
    SENSORS = 2,   // Sensor readings display
    IP = 3,        // Network IP address display
    WIFI_STATION = 4,       // Switch WiFi to Station mode
    WIFI_ACCESS_POINT = 5,  // Switch WiFi to Access Point mode
    WIFI_DISABLED = 6,      // Turn WiFi radio off
    EXIT = 7       // Close menu
};

/**
 * @brief Menu display settings (persisted in RTC memory)
 */
struct __attribute__((packed)) MenuSettings {
    uint32_t textColor = Defaults::Matrix::Menu::TextColor;
    uint16_t scrollSpeed = Defaults::Matrix::Menu::ScrollSpeed;
    bool enabled = Defaults::Matrix::Menu::Enabled;
    uint8_t _pad = 0;
};

/**
 * @brief Matrix LED configuration (persisted in RTC memory)
 */
struct __attribute__((packed)) MatrixData {
    uint8_t brightness = Defaults::Matrix::Brightness;
    MatrixAlarmMode alarmMode = static_cast<MatrixAlarmMode>(Defaults::Matrix::AlarmMode);
    uint8_t rotation = Defaults::Matrix::Rotation;
    bool autoRotate = Defaults::Matrix::AutoRotate;
    
    // Effects
    bool effectEnabled = Defaults::Matrix::EffectEnabled;
    uint8_t effectEngine = Defaults::Matrix::EffectEngine;
    uint8_t effectMode = Defaults::Matrix::EffectMode;
    uint32_t effectSpeed = Defaults::Matrix::EffectSpeed;
    uint32_t effectColor = Defaults::Matrix::EffectColor;
    uint32_t effectColor2 = Defaults::Matrix::EffectColor2;
    uint32_t effectColor3 = Defaults::Matrix::EffectColor3;
    uint8_t effectReactivityProvider = Defaults::Matrix::EffectReactivityProvider;
    uint8_t effectReactivityGain = Defaults::Matrix::EffectReactivityGain;

    // Background mode and sensor data visualization
    uint8_t backgroundMode = Defaults::Matrix::BackgroundMode;
    bool dataVisualizationEnabled = Defaults::Matrix::DataVisualizationEnabled;
    uint8_t dataVisualizationSource = Defaults::Matrix::DataVisualizationSource;
    uint8_t dataVisualizationMetric = Defaults::Matrix::DataVisualizationMetric;
    uint8_t dataVisualizationMode = Defaults::Matrix::DataVisualizationMode;
    float dataVisualizationMin = Defaults::Matrix::DataVisualizationMin;
    float dataVisualizationMax = Defaults::Matrix::DataVisualizationMax;
    uint32_t dataVisualizationColorMin = Defaults::Matrix::DataVisualizationColorMin;
    uint32_t dataVisualizationColorMid = Defaults::Matrix::DataVisualizationColorMid;
    uint32_t dataVisualizationColorMax = Defaults::Matrix::DataVisualizationColorMax;
    uint8_t dataVisualizationBrightnessMin = Defaults::Matrix::DataVisualizationBrightnessMin;
    uint8_t dataVisualizationBrightnessMax = Defaults::Matrix::DataVisualizationBrightnessMax;
    uint8_t dataVisualizationSmoothing = Defaults::Matrix::DataVisualizationSmoothing;
    uint8_t dataVisualizationStaleBehavior = Defaults::Matrix::DataVisualizationStaleBehavior;
    char dataVisualizationDeviceId[MATRIX::kMatrixDataVizDeviceIdCapacity] = {};
    
    // Menu settings
    MenuSettings menu;
};

} // namespace RTC
