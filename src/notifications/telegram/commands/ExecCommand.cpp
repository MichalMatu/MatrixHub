#include "ExecCommand.h"
#include "TelegramReplyBuilder.h"

#include "../../../usb_terminal/UsbTerminalService.h"

#include <cstring>
#include <string>

namespace TELEGRAM::Commands {

bool handleExec(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);

    if (ctx.msg->commandArgs.empty()) {
        reply.header("⌨️", "USB Terminal");
        reply.line("⚠️ Please provide a command to execute.");
        reply.usage("/exec <command>", "Use /exec status or /exec cancel for the active session.");
        reply.finalize();
        return true;
    }

    auto* terminalService = ctx.usbTerminalService;
    if (!terminalService) {
        reply.line("❌ USB Terminal Service not initialized.");
        reply.finalize();
        return true;
    }

    // Forward the command and the origin chat ID so output can be redirected back.
    std::string cmd(ctx.msg->commandArgs);
    terminalService->execute(cmd.c_str(), USB_TERMINAL::SessionTransport::Telegram, ctx.msg->chatId);

    // UsbTerminalService asynchronously pushes the output via MessageQueue.
    reply.finalize(false);
    return true;
}

}  // namespace TELEGRAM::Commands
