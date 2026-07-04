#pragma once

#include "../RtcDefaultValues.h"

namespace RTC {

/**
 * SCD4x temperature compensation tuning parameters.
 * Stored in RTC ConfigStore to survive deep sleep.
 */
struct __attribute__((packed)) CompensationData {
    bool  enabled                = Defaults::Compensation::Enabled;
    float baseTempOffset         = Defaults::Compensation::BaseTempOffset;
    float referenceCpuTemp       = Defaults::Compensation::ReferenceCpuTemp;
    float tempOffsetPerCpuDegree = Defaults::Compensation::TempOffsetPerCpuDegree;
    float minTempOffset          = Defaults::Compensation::MinTempOffset;
    float maxTempOffset          = Defaults::Compensation::MaxTempOffset;
};

} // namespace RTC
