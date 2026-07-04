/**
 * @file RtcConfigLoader.h
 * @brief Boot-path decision for retained config restore vs filesystem reload
 * 
 * This module decides whether boot should:
 * - restore the retained RTC shadow snapshot into the PSRAM working copy, or
 * - rebuild the working copy from defaults + LittleFS config.json
 */

#pragma once

#include <FS.h>

namespace RTC {

/**
 * Initialize RTC configuration system.
 * 
 * Call this early in Application::setup() BEFORE any services that use config.
 * 
 * @param fs Filesystem reference (LittleFS)
 * @return true if configuration is ready (either from RTC or loaded from FS)
 */
bool initConfig(FS& fs);

/**
 * Force reload all configuration from filesystem to RTC.
 * Use after factory reset or config import.
 * 
 * @param fs Filesystem reference (LittleFS)
 * @return true if RTC configuration is ready (defaults are applied even when FS load fails)
 */
bool reloadAllFromFS(FS& fs);

/**
 * Check if this is a warm boot (deep sleep wake) with valid RTC data.
 * Useful for skipping expensive initialization.
 */
bool isWarmBoot();

/**
 * Get boot-path decision latched during initConfig().
 * true  -> warm-boot RTC path was used (FS reload skipped)
 * false -> cold/reload path was used
 */
bool wasWarmBootPathUsed();

}  // namespace RTC
