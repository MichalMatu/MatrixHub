#include "AirMouseConfigJson.h"
#include "../App.h"
#include "../../system/rtc/RtcConfig.h"
#include <algorithm>

namespace CONFIG {
namespace JSON {

// ---------------------------------------------------------------------------
// deserializeAirMouse  —  shared by loadAirMouse (file) AND API endpoint.
// Updates only fields present in JSON (partial-update safe).
// All keys are snake_case, matching the API/frontend format.
// ---------------------------------------------------------------------------
void deserializeAirMouse(JsonObject& obj, RTC::AirMouseData& am) {

    // --- Booleans ---
    if (obj[Keys::kMovementEnabled].is<bool>())       am.movementEnabled = obj[Keys::kMovementEnabled].as<bool>();
    if (obj[Keys::kClickEnabled].is<bool>())           am.clickEnabled = obj[Keys::kClickEnabled].as<bool>();
    if (obj[Keys::kAccelerationEnabled].is<bool>())   am.accelerationEnabled = obj[Keys::kAccelerationEnabled].as<bool>();

    // Legacy transport field is intentionally ignored. AirMouse runtime/boot is
    // USB-owned only, but we still accept old config files without failing load.

    // --- Floats (use temp to avoid packed-field reference binding) ---
    float fv;

    if (obj[Keys::kSensitivityX].is<float>()) {
        fv = std::clamp(obj[Keys::kSensitivityX].as<float>(), LIMITS::AIR_MOUSE::MIN_SENSITIVITY, LIMITS::AIR_MOUSE::MAX_SENSITIVITY);
        am.sensitivityX = fv;
    }
    if (obj[Keys::kSensitivityY].is<float>()) {
        fv = std::clamp(obj[Keys::kSensitivityY].as<float>(), LIMITS::AIR_MOUSE::MIN_SENSITIVITY, LIMITS::AIR_MOUSE::MAX_SENSITIVITY);
        am.sensitivityY = fv;
    }
    if (obj[Keys::kDeadzone].is<float>()) {
        fv = std::clamp(obj[Keys::kDeadzone].as<float>(), LIMITS::AIR_MOUSE::MIN_DEADZONE, LIMITS::AIR_MOUSE::MAX_DEADZONE);
        am.deadzone = fv;
    }
    if (obj[Keys::kAccelerationFactor].is<float>()) {
        fv = std::clamp(obj[Keys::kAccelerationFactor].as<float>(), LIMITS::AIR_MOUSE::MIN_ACCELERATION, LIMITS::AIR_MOUSE::MAX_ACCELERATION);
        am.accelerationFactor = fv;
    }
    if (obj[Keys::kTapThresholdG].is<float>()) {
        fv = std::clamp(obj[Keys::kTapThresholdG].as<float>(), LIMITS::AIR_MOUSE::MIN_TAP_THRESHOLD, LIMITS::AIR_MOUSE::MAX_TAP_THRESHOLD);
        am.tapThresholdG = fv;
    }

    // --- uint16_t ---
    if (obj[Keys::kClickDebounceMs].is<uint16_t>()) {
        uint16_t v = std::clamp(obj[Keys::kClickDebounceMs].as<uint16_t>(), LIMITS::AIR_MOUSE::MIN_DEBOUNCE_MS, LIMITS::AIR_MOUSE::MAX_DEBOUNCE_MS);
        am.clickDebounceMs = v;
    }
    if (obj[Keys::kDoubleClickWindowMs].is<uint16_t>()) {
        uint16_t v = std::clamp(obj[Keys::kDoubleClickWindowMs].as<uint16_t>(), LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS, LIMITS::AIR_MOUSE::MAX_DOUBLE_CLICK_MS);
        am.doubleClickWindowMs = v;
    }

    // --- Click source & actions ---
    if (obj[Keys::kClickSource].is<uint8_t>()) {
        uint8_t v = obj[Keys::kClickSource].as<uint8_t>();
        if (v <= 1) am.clickSource = (RTC::ClickSource)v;
    }
    if (obj[Keys::kSingleClickAction].is<uint8_t>()) {
        uint8_t v = obj[Keys::kSingleClickAction].as<uint8_t>();
        if (v <= 4) am.singleClickAction = (RTC::ClickAction)v;
    }
    if (obj[Keys::kDoubleClickAction].is<uint8_t>()) {
        uint8_t v = obj[Keys::kDoubleClickAction].as<uint8_t>();
        if (v <= 4) am.doubleClickAction = (RTC::ClickAction)v;
    }
    if (obj[Keys::kTripleClickAction].is<uint8_t>()) {
        uint8_t v = obj[Keys::kTripleClickAction].as<uint8_t>();
        if (v <= 4) am.tripleClickAction = (RTC::ClickAction)v;
    }

    if (const char* script = obj[Keys::kSingleClickScript] | (const char*)nullptr) {
        strlcpy(am.singleClickScript, script, sizeof(am.singleClickScript));
    }
    if (const char* script = obj[Keys::kDoubleClickScript] | (const char*)nullptr) {
        strlcpy(am.doubleClickScript, script, sizeof(am.doubleClickScript));
    }
    if (const char* script = obj[Keys::kTripleClickScript] | (const char*)nullptr) {
        strlcpy(am.tripleClickScript, script, sizeof(am.tripleClickScript));
    }

    // --- 1-Euro filter ---
    if (obj[Keys::kEuroMinCutoff].is<float>()) {
        fv = std::clamp(obj[Keys::kEuroMinCutoff].as<float>(), LIMITS::AIR_MOUSE::MIN_EURO_CUTOFF, LIMITS::AIR_MOUSE::MAX_EURO_CUTOFF);
        am.euroMinCutoff = fv;
    }
    if (obj[Keys::kEuroBeta].is<float>()) {
        fv = std::clamp(obj[Keys::kEuroBeta].as<float>(), LIMITS::AIR_MOUSE::MIN_EURO_BETA, LIMITS::AIR_MOUSE::MAX_EURO_BETA);
        am.euroBeta = fv;
    }
    if (obj[Keys::kEuroDCutoff].is<float>()) {
        fv = std::clamp(obj[Keys::kEuroDCutoff].as<float>(), LIMITS::AIR_MOUSE::MIN_EURO_D_CUTOFF, LIMITS::AIR_MOUSE::MAX_EURO_D_CUTOFF);
        am.euroDCutoff = fv;
    }

    // --- Jiggler ---
    if (obj[Keys::kJiggler].is<JsonObject>()) {
        JsonObject j = obj[Keys::kJiggler];

        if (j[Keys::kMode].is<uint8_t>()) {
            uint8_t m = j[Keys::kMode].as<uint8_t>();
            if (m <= 4) am.jiggler.mode = (RTC::MouseJigglerMode)m;
        }
        if (j[Keys::kInterval].is<uint32_t>()) {
            uint32_t v = j[Keys::kInterval].as<uint32_t>();
            // Safety: prevent 0/absurd intervals that could cause tight-loop behavior or long stalls.
            v = std::clamp(v, LIMITS::AIR_MOUSE::MIN_JIGGLER_INTERVAL_S, LIMITS::AIR_MOUSE::MAX_JIGGLER_INTERVAL_S);
            am.jiggler.interval = v;
        }
        if (j[Keys::kMoveDistance].is<uint16_t>()) {
            uint16_t d = j[Keys::kMoveDistance].as<uint16_t>();
            // Safety: clamp to avoid pathological movement values from malformed input.
            d = std::clamp(d, LIMITS::AIR_MOUSE::MIN_JIGGLER_MOVE_DISTANCE, LIMITS::AIR_MOUSE::MAX_JIGGLER_MOVE_DISTANCE);
            am.jiggler.moveDistance = d;
        }
        if (j[Keys::kRandomInterval].is<bool>()) {
            am.jiggler.randomInterval = j[Keys::kRandomInterval].as<bool>();
        }
    }
}

void loadAirMouse(JsonObject& obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::airMouse, [&](RTC::AirMouseData& airMouse) {
        deserializeAirMouse(obj, airMouse);
    });
}

void saveAirMouse(JsonObject& obj) {
    RTC::AirMouseData am{};
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        am = cfg.airMouse;
    });
    
    // Intentionally stop emitting the removed transport key. Once any config is
    // saved again, persisted JSON converges on the new USB-owned contract.
    obj[Keys::kMovementEnabled] = am.movementEnabled;
    obj[Keys::kClickEnabled] = am.clickEnabled;
    obj[Keys::kSensitivityX] = am.sensitivityX;
    obj[Keys::kSensitivityY] = am.sensitivityY;
    obj[Keys::kDeadzone] = am.deadzone;
    obj[Keys::kAccelerationEnabled] = am.accelerationEnabled;
    obj[Keys::kAccelerationFactor] = am.accelerationFactor;
    obj[Keys::kTapThresholdG] = am.tapThresholdG;
    obj[Keys::kClickDebounceMs] = am.clickDebounceMs;
    obj[Keys::kDoubleClickWindowMs] = am.doubleClickWindowMs;
    obj[Keys::kClickSource] = (uint8_t)am.clickSource;
    obj[Keys::kSingleClickAction] = (uint8_t)am.singleClickAction;
    obj[Keys::kDoubleClickAction] = (uint8_t)am.doubleClickAction;
    obj[Keys::kTripleClickAction] = (uint8_t)am.tripleClickAction;
    
    // Convert empty char array to null string in JSON for cleaner API
    if (am.singleClickScript[0] != '\0') obj[Keys::kSingleClickScript].set(String(am.singleClickScript));
    if (am.doubleClickScript[0] != '\0') obj[Keys::kDoubleClickScript].set(String(am.doubleClickScript));
    if (am.tripleClickScript[0] != '\0') obj[Keys::kTripleClickScript].set(String(am.tripleClickScript));
    obj[Keys::kEuroMinCutoff] = am.euroMinCutoff;
    obj[Keys::kEuroBeta] = am.euroBeta;
    obj[Keys::kEuroDCutoff] = am.euroDCutoff;
    
    // Jiggler
    JsonObject j = obj[Keys::kJiggler].to<JsonObject>();
    j[Keys::kMode] = (uint8_t)am.jiggler.mode;
    j[Keys::kInterval] = am.jiggler.interval;
    j[Keys::kMoveDistance] = am.jiggler.moveDistance;
    j[Keys::kRandomInterval] = am.jiggler.randomInterval;
}

} // namespace JSON
} // namespace CONFIG
