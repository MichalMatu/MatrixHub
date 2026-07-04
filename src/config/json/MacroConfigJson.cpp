#include "MacroConfigJson.h"
#include "../App.h"
#include "../../system/logging/Logging.h"
#include "../../system/rtc/RtcConfig.h"
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "JsonMacros"

namespace CONFIG {
namespace JSON {

namespace {

void sanitizeBootScript(const char* input, char* output, size_t outputSize) {
    if (!output || outputSize == 0) return;
    output[0] = '\0';
    if (!input) return;

    char tmp[64];
    strlcpy(tmp, input, sizeof(tmp));

    const char* lastSlash = strrchr(tmp, '/');
    const char* lastBackslash = strrchr(tmp, '\\');
    const char* base = tmp;
    if (lastSlash || lastBackslash) {
        const char* last = lastSlash;
        if (!last || (lastBackslash && lastBackslash > lastSlash)) {
            last = lastBackslash;
        }
        base = last + 1;
    }

    strlcpy(output, base, outputSize);

    // Remove path traversal attempts ("..") even after basename extraction.
    char* p = nullptr;
    while ((p = strstr(output, "..")) != nullptr) {
        memmove(p, p + 2, strlen(p + 2) + 1);
    }
}

} // namespace

// ---------------------------------------------------------------------------
// deserializeMacros — shared by loadMacros (file) AND API endpoint.
// Updates only fields present in JSON (partial-update safe).
// ---------------------------------------------------------------------------
void deserializeMacros(JsonObjectConst obj, RTC::MacroData& cfg) {
    if (obj.isNull()) return;

    if (!obj[CONFIG::Keys::kEnabled].isNull()) {
        bool val = obj[CONFIG::Keys::kEnabled] | RTC::Defaults::Macro::Enabled;
        cfg.enabled = val;
    }
    
    if (!obj[CONFIG::Keys::kBootDelay].isNull()) {
        uint32_t val = obj[CONFIG::Keys::kBootDelay] | RTC::Defaults::Macro::BootDelayMs;
        cfg.bootDelay = val;
    }

    if (!obj[CONFIG::Keys::kBootScript].isNull()) {
        const char* script = obj[CONFIG::Keys::kBootScript];
        if (script) {
            sanitizeBootScript(script, cfg.bootScript, sizeof(cfg.bootScript));
        } else {
            memset(cfg.bootScript, 0, sizeof(cfg.bootScript));
        }
    }

    LOGD("Loaded Macros: enabled=%d, bootDelay=%u, script='%s'", 
        cfg.enabled, cfg.bootDelay, cfg.bootScript);
}

void loadMacros(JsonObjectConst obj) {
    RTC::updateConfigSection(&RTC::ConfigStore::macros, [&](RTC::MacroData& macros) {
        deserializeMacros(obj, macros);
    });
}

void saveMacros(JsonObject obj) {
    RTC::MacroData cfg{};
    RTC::withConfig([&](const RTC::ConfigStore& store) {
        cfg = store.macros;
    });

    obj[CONFIG::Keys::kEnabled] = cfg.enabled;
    obj[CONFIG::Keys::kBootDelay] = cfg.bootDelay;
    
    if (strlen(cfg.bootScript) > 0) {
        obj[CONFIG::Keys::kBootScript].set(String(cfg.bootScript)); // duplicate to Json pool
    } else {
        obj[CONFIG::Keys::kBootScript] = "";
    }
}

} // namespace JSON
} // namespace CONFIG
