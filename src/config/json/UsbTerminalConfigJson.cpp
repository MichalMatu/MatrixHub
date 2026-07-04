#include "UsbTerminalConfigJson.h"
#include "../App.h"
#include "../../system/rtc/RtcConfig.h"

namespace CONFIG {
namespace JSON {

void deserializeUsbTerminal(JsonObject& obj, RTC::UsbTerminalData& data) {
    if (obj[Keys::kEnabled].is<bool>()) {
        bool v = obj[Keys::kEnabled].as<bool>();
        data.enabled = v;
    }
    if (const char* port = obj[Keys::kTargetPort] | (const char*)nullptr) {
        // Use temporary aligned buffer to avoid unaligned access on packed struct
        char tempPort[sizeof(data.targetPort)] = {0};
        strlcpy(tempPort, port, sizeof(tempPort));
        memcpy(data.targetPort, tempPort, sizeof(data.targetPort));
    }
    if (obj[Keys::kIdleTimeoutMs].is<uint32_t>()) {
        uint32_t timeout = obj[Keys::kIdleTimeoutMs].as<uint32_t>();
        
        // Clamp timeout 
        if (timeout < TIMEOUT::USB_TERMINAL::MIN_IDLE_TIMEOUT_MS) {
             timeout = TIMEOUT::USB_TERMINAL::MIN_IDLE_TIMEOUT_MS;
        } else if (timeout > TIMEOUT::USB_TERMINAL::MAX_IDLE_TIMEOUT_MS) {
             timeout = TIMEOUT::USB_TERMINAL::MAX_IDLE_TIMEOUT_MS;
        }
        data.idleTimeoutMs = timeout;
    }
}

void loadUsbTerminal(JsonObject& obj) {
    if (obj.isNull()) return;
    RTC::updateConfigSection(&RTC::ConfigStore::usbTerminal, [&](RTC::UsbTerminalData& usbTerminal) {
        deserializeUsbTerminal(obj, usbTerminal);
    });
}

void saveUsbTerminal(JsonObject& obj) {
    RTC::UsbTerminalData currentCfg = RTC::copyConfigSection(&RTC::ConfigStore::usbTerminal);

    obj[Keys::kEnabled] = currentCfg.enabled;
    obj[Keys::kIdleTimeoutMs] = currentCfg.idleTimeoutMs;
    
    // targetPort is in a packed struct, copy to aligned buffer before passing to ArduinoJson
    char tempPort[sizeof(currentCfg.targetPort)] = {0};
    memcpy(tempPort, currentCfg.targetPort, sizeof(tempPort));
    tempPort[sizeof(tempPort) - 1] = '\0';
    obj[Keys::kTargetPort].set(String(tempPort)); // duplicate into JsonDocument
}

} // namespace JSON
} // namespace CONFIG
