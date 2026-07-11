/**
 * @file AlarmRulesSerializer.cpp
 * @brief JSON serialization logic for alarm rules
 */

#include "AlarmRulesSerializer.h"
#include "../../../config/App.h"
#include "../../../system/boot/BootId.h"
#include "../../../system/utils/json/JsonResponseWriter.h"
#include <Arduino.h>
#include <cmath>

namespace API {

bool AlarmRulesSerializer::serialize(Utils::JsonResponseWriter& w, 
                                    const ALARMS::AlarmRule* rules, 
                                    uint8_t count,
                                    const RuleStatus* statuses,
                                    bool includeStatus) {
    if (!rules && count > 0) return false;

    if (!w.raw("{")) return false;
    if (!w.key(CONFIG::Keys::kSchemaVersion)) return false;
    if (!w.value(1)) return false;
    if (!w.raw(",")) return false;
    if (!w.key(CONFIG::Keys::kRules)) return false;
    if (!w.raw("[")) return false;

    bool firstRule = true;
    for (uint8_t i = 0; i < count; i++) {
        const ALARMS::AlarmRule& rule = rules[i];
        if (!rule.isValid()) continue;

        if (!firstRule) if (!w.raw(",")) return false;
        firstRule = false;

        if (!w.raw("{")) return false;

        if (!w.key("id") || !w.string(rule.id)) return false;
        if (!w.raw(",") || !w.key("name") || !w.string(rule.name)) return false;
        if (!w.raw(",") || !w.key("enabled") || !w.value(rule.enabled)) return false;
        if (!w.raw(",") || !w.key("source") || !w.string(ALARMS::sourceToString(rule.source))) return false;
        if (!w.raw(",") || !w.key("operator") || !w.string(ALARMS::operatorToString(rule.op))) return false;
        if (!w.raw(",") || !w.key("threshold") || !w.value((float)rule.threshold, 3)) return false;
        if (!w.raw(",") || !w.key("severity") || !w.string(ALARMS::severityToString(rule.severity))) return false;
        if (!w.raw(",") || !w.key("cooldown_seconds") || !w.value((unsigned int)rule.cooldownSeconds)) return false;
        if (!w.raw(",") || !w.key("created_at") || !w.value((unsigned long)rule.createdAt)) return false;
        if (!w.raw(",") || !w.key("updated_at") || !w.value((unsigned long)rule.updatedAt)) return false;
        
        // Add BLE device MAC for BLE sources
        if (rule.isBleSource() && rule.bleDeviceMac[0] != '\0') {
            if (!w.raw(",") || !w.key("ble_device_mac") || !w.string(rule.bleDeviceMac)) return false;
        }

        if (rule.isGpioSource() && rule.gpioId[0] != '\0') {
            if (!w.raw(",") || !w.key(CONFIG::Keys::kGpioId) || !w.string(rule.gpioId)) return false;
        }

        if (includeStatus) {
            const RuleStatus* status = statuses ? &statuses[i] : nullptr;
            const bool hasStatus = status && status->valid;
            const bool triggered = hasStatus ? status->triggered : false;
            const uint32_t lastTriggered = hasStatus ? status->lastTriggered : 0;
            const float currentValue = hasStatus ? status->currentValue : NAN;
            const uint32_t transitionSeq = hasStatus ? status->transitionSeq : 0;
            const uint32_t deviceMillis = hasStatus ? status->deviceMillis : 0;
            const uint64_t bootId = hasStatus ? status->bootId : 0;
            char bootIdHex[SYSTEM::kBootIdHexLength + 1];
            SYSTEM::formatBootIdHex(bootId, bootIdHex);

            if (!w.raw(",") || !w.key("triggered") || !w.value(triggered)) return false;
            if (!w.raw(",") || !w.key("last_triggered") || !w.value((unsigned long)lastTriggered)) return false;
            if (!w.raw(",") || !w.key("transition_seq") || !w.value((unsigned long)transitionSeq)) return false;
            if (!w.raw(",") || !w.key("device_millis") || !w.value((unsigned long)deviceMillis)) return false;
            if (!w.raw(",") || !w.key("boot_id") || !w.string(bootIdHex)) return false;
            
            // Add current sensor value for this rule
            if (!std::isnan(currentValue)) {
                if (!w.raw(",") || !w.key("current_value") || !w.value((float)currentValue, 2)) return false;
            }
        }

        // notifyChannels
        if (!w.raw(",") || !w.key("notify_channels") || !w.raw("[")) return false;
        bool firstCh = true;
        if (ALARMS::hasChannel(rule.notifyChannels, ALARMS::NotifyChannel::Telegram)) {
            if (!firstCh) if (!w.raw(",")) return false;
            if (!w.string("telegram")) return false;
            firstCh = false;
        }
        if (ALARMS::hasChannel(rule.notifyChannels, ALARMS::NotifyChannel::Led)) {
            if (!firstCh) if (!w.raw(",")) return false;
            if (!w.string("led")) return false;
            firstCh = false;
        }
        if (ALARMS::hasChannel(rule.notifyChannels, ALARMS::NotifyChannel::Webhook)) {
            if (!firstCh) if (!w.raw(",")) return false;
            if (!w.string("webhook")) return false;
            firstCh = false;
        }
        if (ALARMS::hasChannel(rule.notifyChannels, ALARMS::NotifyChannel::Pushover)) {
            if (!firstCh) if (!w.raw(",")) return false;
            if (!w.string("pushover")) return false;
            firstCh = false;
        }
        if (!w.raw("]")) return false;

        // shellyDeviceIds
        if (rule.shellyDeviceCount > 0) {
            if (!w.raw(",") || !w.key(CONFIG::Keys::kShellyDeviceIds) || !w.raw("[")) return false;
            bool firstDev = true;
            for (uint8_t j = 0; j < rule.shellyDeviceCount && j < ALARMS::kMaxShellyPerRule; j++) {
                if (rule.shellyDeviceIds[j][0] == '\0') continue;
                if (!firstDev) if (!w.raw(",")) return false;
                firstDev = false;
                if (!w.string(rule.shellyDeviceIds[j])) return false;
            }
            if (!w.raw("]")) return false;
        }

        if (!w.raw("}")) return false;
    }

    if (!w.raw("]}")) return false;
    return true;
}

} // namespace API
