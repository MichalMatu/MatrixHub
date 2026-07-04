/**
 * @file RebootCommand.cpp
 * @brief Implementation of /reboot command
 */

#include "RebootCommand.h"
#include "TelegramReplyBuilder.h"

#include "../../../config/System.h"
#include "../../../system/boot/BootTracker.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/shutdown/ShutdownSequence.h"
#include "../../../system/utils/Random.h"

#include <Arduino.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string>

namespace {
constexpr const char* kLogTag = "TgReboot";

void logRebootError(const char* message) {
#if defined(NATIVE_BUILD)
    (void)message;
#else
    LOG::Logging::log(ESP_LOG_ERROR, kLogTag, "%s", message);
#endif
}
}

namespace TELEGRAM::Commands {

static uint32_t s_expectedRebootPin = 0;
static void delayedRebootTask(void* pvParameters);

static bool scheduleDelayedRebootTask(ServiceRegistry* registry, TaskHandle_t* outTaskHandle = nullptr) {
    TaskHandle_t taskHandle = nullptr;
    const BaseType_t result = xTaskCreatePinnedToCore(
        delayedRebootTask,
        "reboot_task",
        CONFIG::TASKS::STACK_REBOOT,
        registry,
        CONFIG::TASKS::PRIO_NOTIFICATION,
        &taskHandle,
        CONFIG::TASKS::CORE_NOTIFICATION);

    if (outTaskHandle) {
        *outTaskHandle = taskHandle;
    }

    return result == pdPASS && taskHandle != nullptr;
}

static void delayedRebootTask(void* pvParameters) {
    auto* registry = static_cast<ServiceRegistry*>(pvParameters);

    // Wait for the Telegram response to be sent and for the next getUpdates poll
    // to acknowledge the command offset to the Telegram server.
    vTaskDelay(pdMS_TO_TICKS(TIMEOUT::REBOOT_COMMAND_DELAY_MS));

    if (!registry) {
        logRebootError("Reboot task started without ServiceRegistry; aborting restart");
        vTaskDelete(NULL);
        return;
    }

    ::SYSTEM::ShutdownSequence::execute(*registry, ::SYSTEM::ShutdownReason::RESTART_COMMAND);
    esp_restart();
    vTaskDelete(NULL);
}

bool handleReboot(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("♻️", "Restart Device");

    if (ctx.msg->commandArgs.empty()) {
        s_expectedRebootPin = UTILS::RNG::rangeU32(1000u, 9999u);

        reply.line("⚠️ This will restart the device.");
        reply.detailf("Confirm with: /reboot %lu", s_expectedRebootPin);
        reply.finalize();
        return true;
    }

    std::string argStr(ctx.msg->commandArgs);
    uint32_t providedPin = strtoul(argStr.c_str(), nullptr, 10);

    if (providedPin == 0 || providedPin != s_expectedRebootPin) {
        if (s_expectedRebootPin == 0) {
            reply.line("✅ Device successfully restarted.");
        } else {
            reply.line("❌ Invalid or expired reboot code.");
        }
        reply.finalize();
        s_expectedRebootPin = 0;
        return true;
    }

    if (!ctx.registry) {
        s_expectedRebootPin = 0;
        reply.line("❌ Restart unavailable: service registry not available.");
        reply.detailf("Request a new confirmation code with /reboot.");
        reply.finalize();
        return true;
    }

    TaskHandle_t rebootTaskHandle = nullptr;
    if (!scheduleDelayedRebootTask(ctx.registry, &rebootTaskHandle)) {
        s_expectedRebootPin = 0;
        reply.line("❌ Failed to schedule reboot.");
        reply.detailf("Request a new confirmation code with /reboot.");
        reply.finalize();
        return true;
    }

    s_expectedRebootPin = 0;

    reply.line("🔄 Device will reboot in a moment...");
    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
