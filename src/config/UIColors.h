/**
 * @file UIColors.h
 * @brief Centralized Color Definitions for the Matrix UI
 * 
 * All colors are defined in Native RGB888 (uint32_t) format: 0xRRGGBB.
 */

#pragma once

#include <cstdint>

namespace UI {
namespace COLOR {
    // Semantic Colors (Alarms)
    constexpr uint32_t ALARM_INFO     = 0x0000FF; // BLUE   - Used for minor notifications, e.g. "Low Water Info"
    constexpr uint32_t ALARM_WARNING  = 0xFFFF00; // YELLOW - Used for issues needing attention soon, e.g. "Water Soon"
    constexpr uint32_t ALARM_CRITICAL = 0xFF0000; // RED    - Used for severe system-level alerts, e.g. "Dry Pump"
    
    // Semantic Colors (Macros)
    constexpr uint32_t MACRO_PRINT    = 0x00FF00; // GREEN  - Displayed temporarily when executing a standard print command
    constexpr uint32_t MACRO_RUN      = 0xFFA500; // ORANGE - Displayed temporarily when launching a complex executable macro script
    
    // Semantic Colors (Menu / System)
    constexpr uint32_t MENU_TEXT      = 0xFFFFFF; // WHITE  - Default color of navigation menu (Time, IP, Sensors) 
    constexpr uint32_t TELEGRAM_CMD   = 0xFFFFFF; // WHITE  - Text color when using Telegram /matrix echo command

} // namespace COLOR
} // namespace UI
