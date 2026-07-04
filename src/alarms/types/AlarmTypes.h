/**
 * @file AlarmTypes.h
 * @brief Aggregating header for all Alarm types
 * 
 * Include this file for full access to all alarm types.
 * Individual headers can be included for minimal dependencies.
 * 
 * Structure:
 *   types/AlarmConstants.h     - Compile-time constants (kMaxRules, etc.)
 *   types/AlarmEnums.h         - Enum types (AlarmSource, AlarmSeverity, etc.)
 *   types/AlarmRule.h          - AlarmRule structure
 *   types/AlarmEvent.h         - AlarmEvent structure (for history)
 *   types/AlarmRuntimeState.h  - Runtime tracking state
 *   serialization/AlarmEnumConverters.h  - Enum <-> String conversions
 */

#pragma once

// Core types
#include "AlarmConstants.h"
#include "AlarmEnums.h"
#include "AlarmInputData.h"
#include "AlarmRule.h"
#include "AlarmEvent.h"
#include "AlarmRuntimeState.h"

// Serialization helpers
#include "../serialization/AlarmEnumConverters.h"
