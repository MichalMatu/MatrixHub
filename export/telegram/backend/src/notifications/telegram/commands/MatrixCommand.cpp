/**
 * @file MatrixCommand.cpp
 * @brief Implementation of /matrix command
 */

#include "MatrixCommand.h"
#include "TelegramReplyBuilder.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "../../../config/UIColors.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/matrix_manager/MatrixManagerService.h"
#include "../../../system/matrix_manager/MatrixManagerTypes.h"

namespace TELEGRAM::Commands {

bool handleMatrix(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("🟪", "Matrix Display");

    if (!ctx.matrixManager) {
        reply.line("❌ Matrix Manager is not available.");
        reply.finalize();
        return true;
    }

    if (ctx.msg->commandArgs.empty()) {
        reply.usage("/matrix <text>");
        reply.finalize();
        return true;
    }

    // To repeat 3 times, we concatenate the string with some spacing
    // e.g. "Text   Text   Text"
    MATRIX_MANAGER::LayerContent content;
    content.type = CommandType::SHOW_TEXT;
    content.color = UI::COLOR::TELEGRAM_CMD;

    // Matrix scroll speed is typically ~50-80ms per pixel.
    // We set a safe 15-second timeout, after which the Manager auto-clears it.
    content.durationMs = 15000;

    // Safely build the looping string "arg   arg   arg"
    std::string_view arg = ctx.msg->commandArgs;
    const size_t maxSegLen = 25;
    const int segLen = static_cast<int>(std::min(arg.length(), maxSegLen));
    snprintf(content.text, sizeof(content.text), "%.*s   %.*s   %.*s",
             segLen, arg.data(), segLen, arg.data(), segLen, arg.data());

    // Set it directly to the SYSTEM_MODAL layer (priority 4, higher than Alarms)
    ctx.matrixManager->setLayer(MATRIX_MANAGER::Layer::SYSTEM_MODAL, content);

    // Send confirmation back
    reply.line("✅ Message sent to display.");
    reply.detailf("Priority: system modal");
    reply.detailf("Text: %.*s", (int)arg.length(), arg.data());
    reply.finalize();

    return true;
}

}  // namespace TELEGRAM::Commands
