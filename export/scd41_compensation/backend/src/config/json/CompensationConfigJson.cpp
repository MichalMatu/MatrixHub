#include "CompensationConfigJson.h"
#include "../App.h"
#include "../../system/rtc/RtcConfig.h"
#include "../System.h"
#include <algorithm>

namespace CONFIG {
namespace JSON {

void deserializeCompensation(JsonObject& obj, RTC::CompensationData& data) {
    using namespace LIMITS::COMPENSATION;

    if (obj[Keys::kEnabled].is<bool>()) {
        data.enabled = obj[Keys::kEnabled].as<bool>();
    }
    if (obj[Keys::kBaseTempOffset].is<float>()) {
        float val = obj[Keys::kBaseTempOffset].as<float>();
        data.baseTempOffset = std::clamp(val, MIN_BASE_OFFSET, MAX_BASE_OFFSET);
    }
    if (obj[Keys::kReferenceCpuTemp].is<float>()) {
        float val = obj[Keys::kReferenceCpuTemp].as<float>();
        data.referenceCpuTemp = std::clamp(val, MIN_REF_CPU_TEMP, MAX_REF_CPU_TEMP);
    }
    if (obj[Keys::kTempOffsetPerCpuDegree].is<float>()) {
        float val = obj[Keys::kTempOffsetPerCpuDegree].as<float>();
        data.tempOffsetPerCpuDegree = std::clamp(val, MIN_SLOPE, MAX_SLOPE);
    }
    if (obj[Keys::kMinTempOffset].is<float>()) {
        float val = obj[Keys::kMinTempOffset].as<float>();
        data.minTempOffset = std::clamp(val, MIN_OFFSET_CLAMP, MAX_OFFSET_CLAMP);
    }
    if (obj[Keys::kMaxTempOffset].is<float>()) {
        float val = obj[Keys::kMaxTempOffset].as<float>();
        data.maxTempOffset = std::clamp(val, MIN_OFFSET_CLAMP, MAX_OFFSET_CLAMP);
    }
    
    // Safety check: ensure min <= max
    if (data.minTempOffset > data.maxTempOffset) {
        data.minTempOffset = data.maxTempOffset;
    }
}

void loadCompensation(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::compensation, [&](RTC::CompensationData& compensation) {
        deserializeCompensation(obj, compensation);
    });
}

void saveCompensation(JsonObject& obj) {
    RTC::CompensationData c = RTC::getConfig().compensation;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        c = cfg.compensation;
    });
    obj[Keys::kEnabled] = c.enabled;
    obj[Keys::kBaseTempOffset] = c.baseTempOffset;
    obj[Keys::kReferenceCpuTemp] = c.referenceCpuTemp;
    obj[Keys::kTempOffsetPerCpuDegree] = c.tempOffsetPerCpuDegree;
    obj[Keys::kMinTempOffset] = c.minTempOffset;
    obj[Keys::kMaxTempOffset] = c.maxTempOffset;
}

} // namespace JSON
} // namespace CONFIG
