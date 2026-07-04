/**
 * @file HelpCommand.cpp
 * @brief Implementation of /help command
 */

#include "HelpCommand.h"
#include "CommandRegistry.h"
#include "TelegramReplyBuilder.h"

#include <cstdio>
#include <cstring>

namespace TELEGRAM::Commands {

namespace {

void appendHelpEntry(TelegramReplyBuilder& reply, const char* command, const char* description, bool nested = false) {
    if (nested) {
        reply.linef("    %s - %s", command, description);
        return;
    }
    reply.detailf("%s - %s", command, description);
}

const CommandDef* findCommand(const char* name) {
    if (!name) {
        return nullptr;
    }

    for (size_t i = 0; i < kCommandCount; i++) {
        if (strcmp(kCommands[i].name, name) == 0) {
            return &kCommands[i];
        }
    }

    return nullptr;
}

void appendRegistryCommand(TelegramReplyBuilder& reply, const CommandDef& command) {
    reply.detailf("/%s - %s", command.name, command.description);
}

void appendGroup(TelegramReplyBuilder& reply, HelpGroup group, const char* icon, const char* title) {
    bool wroteAny = false;
    for (size_t i = 0; i < kCommandCount; i++) {
        if (kCommands[i].group != group) {
            continue;
        }

        if (!wroteAny) {
            reply.section(icon, title);
            wroteAny = true;
        }

        appendRegistryCommand(reply, kCommands[i]);
    }
}

}  // namespace

bool handleHelp(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("📋", "Available Commands");

    if (const CommandDef* help = findCommand("help")) {
        appendRegistryCommand(reply, *help);
        reply.line();
    }

    appendGroup(reply, HelpGroup::StatusDiagnostics, "📡", "Status & Diagnostics");
    reply.line();

    appendGroup(reply, HelpGroup::FeaturesDevices, "🔌", "Features & Devices");
    reply.line();

    appendGroup(reply, HelpGroup::Automation, "🤖", "Automation");
    reply.line();

    appendGroup(reply, HelpGroup::ToolsSystem, "🛠️", "Tools & System");
    appendHelpEntry(reply, "/exec cancel", "Send Ctrl+C to host", true);
    appendHelpEntry(reply, "/exec status", "Peek at command capture buffer", true);

    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
