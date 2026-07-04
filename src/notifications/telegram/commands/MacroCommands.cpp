/**
 * @file MacroCommands.cpp
 * @brief Implementation of /scripts, /run, /macro_stop commands
 */

#include "MacroCommands.h"
#include "TelegramReplyBuilder.h"

#include "../../../macros/MacroService.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace TELEGRAM::Commands {

bool handleScripts(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("📜", "Macro Scripts");

    if (!ctx.macroService) {
        reply.line("❌ Macro module not available.");
        reply.finalize();
        return true;
    }

    auto scripts = ctx.macroService->listScripts();

    if (scripts.empty()) {
        reply.line("No macro scripts found.");
        reply.finalize();
        return true;
    }

    reply.kvf("📊", "Available", "%u", (unsigned)scripts.size());
    reply.line();

    for (size_t i = 0; i < scripts.size() && !reply.isTruncated(); i++) {
        reply.bulletf("%u. %s", (unsigned)(i + 1), scripts[i].c_str());
    }

    if (!reply.isTruncated()) {
        reply.line();
        reply.detailf("Run with: /run <name>");
    }

    reply.finalize();
    return true;
}

bool handleRun(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("▶️", "Run Macro");

    if (!ctx.macroService) {
        reply.line("❌ Macro module not available.");
        reply.finalize();
        return true;
    }

    if (ctx.msg->commandArgs.empty()) {
        reply.usage("/run <script_name>", "Use /scripts to see available scripts.");
        reply.finalize();
        return true;
    }

    std::string scriptName(ctx.msg->commandArgs);
    bool ok = ctx.macroService->startScript(scriptName.c_str());

    if (ok) {
        reply.line("✅ Macro started.");
        reply.detailf("Script: %s", scriptName.c_str());
    } else {
        reply.line("❌ Failed to start macro.");
        reply.detailf("Script: %s", scriptName.c_str());
        reply.detailf("Check the name with /scripts.");
    }

    reply.finalize();
    return true;
}

bool handleMacroStop(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("⏹️", "Macro Control");

    if (!ctx.macroService) {
        reply.line("❌ Macro module not available.");
        reply.finalize();
        return true;
    }

    auto status = ctx.macroService->getStatus();

    if (status.status != MACROS::MacroStatus::RUNNING) {
        reply.line("ℹ️ No macro is currently running.");
        reply.finalize();
        return true;
    }

    ctx.macroService->stopScript();

    reply.line("✅ Macro stopped.");
    reply.detailf("Script: %s", status.currentScript.c_str());
    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
