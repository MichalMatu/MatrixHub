/**
 * @file AlarmsCommand.cpp
 * @brief Implementation of /alarms command
 */

#include "AlarmsCommand.h"
#include "TelegramReplyBuilder.h"

#include <cstdio>

#include "../../../alarms/AlarmRulesStore.h"
#include "../../../system/memory/SystemAllocator.h"

namespace TELEGRAM::Commands {

// Helper to convert AlarmSource enum to short string
static const char* sourceToStr(ALARMS::AlarmSource src) {
    switch (src) {
        case ALARMS::AlarmSource::CO2:         return "CO2";
        case ALARMS::AlarmSource::Temperature: return "Temp";
        case ALARMS::AlarmSource::Humidity:    return "Hum";
        case ALARMS::AlarmSource::WifiMotion:  return "Motion";
        default:                               return "?";
    }
}

// Helper to convert AlarmOperator enum to symbol
static const char* opToStr(ALARMS::AlarmOperator op) {
    switch (op) {
        case ALARMS::AlarmOperator::Above: return ">";
        case ALARMS::AlarmOperator::Below: return "<";
        default:                           return "?";
    }
}

// Helper to convert AlarmSeverity enum to emoji
static const char* severityToEmoji(ALARMS::AlarmSeverity sev) {
    switch (sev) {
        case ALARMS::AlarmSeverity::Info:     return "ℹ️";
        case ALARMS::AlarmSeverity::Warning:  return "⚠️";
        case ALARMS::AlarmSeverity::Critical: return "🚨";
        default:                              return "❓";
    }
}

bool handleAlarms(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("🔔", "Alarm Rules");

    // Read-only alarm snapshots are plain CPU data, so a temporary PSRAM
    // buffer is safe and avoids a large stack object in the Telegram worker.
    // Before: this command stored AlarmRulesData directly on the worker stack.
    auto alarms = SYSTEM::MEMORY::makeUniqueInPsram<ALARMS::AlarmRulesSnapshot>();
    const bool haveSnapshot = alarms && ALARMS::RULES_CONFIG::copyTo(*alarms);

    if (!haveSnapshot) {
        reply.line("Alarm rules unavailable.");
    } else if (alarms->ruleCount == 0) {
        reply.line("No alarm rules configured.");
    } else {
        reply.kvf("📊", "Configured", "%u", alarms->ruleCount);
        reply.line();

        for (uint8_t i = 0; i < alarms->ruleCount && !reply.isTruncated(); i++) {
            const auto& rule = alarms->rules[i];
            if (!rule.isValid()) continue;

            reply.bulletf("%s [%s] %s", severityToEmoji(rule.severity), rule.enabled ? "ON" : "OFF", rule.name);
            if (!reply.isTruncated()) {
                reply.detailf("%s %s %.1f", sourceToStr(rule.source), opToStr(rule.op), rule.threshold);
            }
        }
    }

    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
