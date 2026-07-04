#include "ShellyCommand.h"
#include "TelegramReplyBuilder.h"

#include "../../../shelly/ShellyConfigStore.h"
#include "../../../shelly/ShellyService.h"

#include <cstdio>
#include <cstring>

namespace TELEGRAM::Commands {

// Find device ID by IP address (returns nullptr if not found)
static const char* findDeviceIdByIp(const char* ip) {
    static RTC::ShellyData shelly;
    shelly = SHELLY::CONFIG_STORE::copy();
    for (uint8_t i = 0; i < shelly.deviceCount; i++) {
        if (strcmp(shelly.devices[i].ip, ip) == 0) {
            return shelly.devices[i].id;
        }
    }
    return nullptr;
}

// Parse "on"/"off" argument (case-insensitive)
static bool parseOnOff(const char* arg, bool& turnOn) {
    if (!arg) return false;

    if (strcasecmp(arg, "on") == 0 || strcmp(arg, "1") == 0) {
        turnOn = true;
        return true;
    }
    if (strcasecmp(arg, "off") == 0 || strcmp(arg, "0") == 0) {
        turnOn = false;
        return true;
    }
    return false;
}

// Handle relay control: /shelly <ip> on|off
static bool handleControl(CommandContext& ctx, std::string_view args) {
    TelegramReplyBuilder reply(ctx);
    reply.header("🔌", "Shelly Control");

    // Parse IP and state from args
    char ipBuf[20] = {0};
    char stateBuf[8] = {0};

    // Simple parsing: "192.168.1.50 on"
    size_t spacePos = args.find(' ');
    if (spacePos == std::string_view::npos) {
        reply.usage("/shelly <ip> on|off", "Example: /shelly 192.168.1.50 on");
        reply.finalize();
        return true;
    }

    std::string_view ipPart = args.substr(0, spacePos);
    std::string_view rest = args.substr(spacePos);

    // Trim leading spaces from rest
    while (!rest.empty() && rest[0] == ' ') rest.remove_prefix(1);

    if (ipPart.length() >= sizeof(ipBuf)) ipPart = ipPart.substr(0, sizeof(ipBuf) - 1);
    memcpy(ipBuf, ipPart.data(), ipPart.length());
    ipBuf[ipPart.length()] = '\0';

    std::string_view statePart = rest;
    if (statePart.length() >= sizeof(stateBuf)) statePart = statePart.substr(0, sizeof(stateBuf) - 1);
    memcpy(stateBuf, statePart.data(), statePart.length());
    stateBuf[statePart.length()] = '\0';

    // Parse on/off
    bool turnOn;
    if (!parseOnOff(stateBuf, turnOn)) {
        reply.linef("❌ Invalid state: %s", stateBuf);
        reply.detailf("Use: on, off, 1, or 0.");
        reply.finalize();
        return true;
    }

    // Find device by IP
    const char* deviceId = findDeviceIdByIp(ipBuf);
    if (!deviceId) {
        reply.linef("❌ No device found at IP: %s", ipBuf);
        reply.detailf("Use /shelly to list configured devices.");
        reply.finalize();
        return true;
    }

    // Get ShellyService from injected context
    auto* shellyService = ctx.shellyService;
    if (!shellyService) {
        reply.line("❌ Shelly service not available.");
        reply.finalize();
        return true;
    }

    bool queued = shellyService->setRelayState(deviceId, turnOn);

    if (queued) {
        reply.line("✅ Command queued.");
        reply.detailf("Target: %s", ipBuf);
        reply.detailf("State: %s", turnOn ? "ON" : "OFF");
    } else {
        reply.line("⚠️ Failed to queue command.");
        reply.detailf("Target: %s", ipBuf);
    }

    reply.finalize();
    return true;
}

// Handle list: /shelly (no args)
static bool handleList(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("🔌", "Shelly Devices");

    const RTC::ShellyData shelly = SHELLY::CONFIG_STORE::copy();

    if (shelly.deviceCount == 0) {
        reply.line("No Shelly devices configured.");
    } else {
        reply.kvf("📊", "Configured", "%u", shelly.deviceCount);
        reply.line();

        for (uint8_t i = 0; i < shelly.deviceCount && !reply.isTruncated(); i++) {
            const auto& dev = shelly.devices[i];
            if (!dev.isValid()) continue;

            reply.bulletf("%s %s", dev.enabled ? "🟢" : "⏸️", dev.name[0] ? dev.name : dev.id);
            if (!reply.isTruncated()) {
                reply.detailf("📍 %s (relay %u)", dev.ip, dev.relayIndex);
            }
        }

        if (!reply.isTruncated()) {
            reply.line();
            reply.detailf("Control: /shelly <ip> on|off");
        }
    }

    reply.finalize();
    return true;
}

bool handleShelly(CommandContext& ctx) {
    std::string_view args = ctx.msg->commandArgs;

    // No args = list devices
    if (args.empty()) {
        return handleList(ctx);
    }

    // Has args = control command
    return handleControl(ctx, args);
}

}  // namespace TELEGRAM::Commands
