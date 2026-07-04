/**
 * @file TriggeredCommand.cpp
 * @brief Implementation of /triggered command - shows currently active alarms
 */

#include "TriggeredCommand.h"
#include "TelegramReplyBuilder.h"

#include <cstdio>

#include "../../../alarms/AlarmService.h"
#include "../../../system/utils/ScopeLock.h"

namespace TELEGRAM::Commands {

// Helper to format time ago
static void formatTimeAgo(uint32_t deltaMs, char* buf, size_t bufSize) {
    uint32_t sec = deltaMs / 1000;
    if (sec < 60) {
        snprintf(buf, bufSize, "%us ago", sec);
    } else if (sec < 3600) {
        snprintf(buf, bufSize, "%um ago", sec / 60);
    } else {
        snprintf(buf, bufSize, "%uh ago", sec / 3600);
    }
}

bool handleTriggered(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("🚨", "Active Alarms");

    // Use injected alarm service
    if (!ctx.alarmService) {
        reply.line("⚠️ Alarm service unavailable.");
        reply.finalize();
        return true;
    }

    auto& manager = ctx.alarmService->getManager();

    SYSTEM::ScopeLock managerLock(manager.getMutex(), pdMS_TO_TICKS(500));
    if (!managerLock.isLocked()) {
        reply.line("⚠️ Alarm system busy.");
        reply.finalize();
        return true;
    }

    const auto* rules = manager.getRules();
    auto* states = manager.getStates();
    uint8_t count = manager.getCount();

    // Count triggered alarms
    uint8_t triggeredCount = 0;
    for (uint8_t i = 0; i < count; i++) {
        if (rules[i].enabled && states[i].previouslyTriggered) {
            triggeredCount++;
        }
    }

    if (triggeredCount == 0) {
        reply.line("✅ No active alarms.");
        reply.kvf("📊", "Rules monitored", "%u", count);
    } else {
        reply.kvf("📊", "Triggered", "%u", triggeredCount);
        reply.line();

        uint32_t now = millis();

        for (uint8_t i = 0; i < count && !reply.isTruncated(); i++) {
            if (!rules[i].enabled || !states[i].previouslyTriggered) continue;

            // Format time since trigger
            char timeAgo[16] = "now";
            if (states[i].lastTriggeredMs > 0 && now >= states[i].lastTriggeredMs) {
                formatTimeAgo(now - states[i].lastTriggeredMs, timeAgo, sizeof(timeAgo));
            }

            reply.bulletf("%s", rules[i].name);
            if (!reply.isTruncated()) {
                reply.detailf("⏱ Triggered %s", timeAgo);
            }
        }
    }

    managerLock.unlock();

    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
