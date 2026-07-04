/**
 * @file UsersCommand.cpp
 * @brief Implementation of /users command
 */

#include "UsersCommand.h"
#include "TelegramReplyBuilder.h"

#include <security/SecuritySettingsService.h>
#include <cstdio>

namespace TELEGRAM::Commands {

bool handleUsers(CommandContext& ctx) {
    TelegramReplyBuilder reply(ctx);
    reply.header("👥", "System Users");

    if (!ctx.securityManager) {
        reply.line("❌ Security manager not available.");
    } else {
        // The shared command context exposes SecurityManager*. Narrow it here
        // because /users is a privileged local admin utility.
        auto* settingsService = static_cast<SecuritySettingsService*>(ctx.securityManager);

        settingsService->read([&](SecuritySettings& settings) {
            if (settings.users.empty()) {
                reply.line("No users found.");
            } else {
                reply.kvf("📊", "Users", "%u", (unsigned)settings.users.size());
                reply.line();

                for (const auto& user : settings.users) {
                    if (reply.isTruncated()) {
                        break;
                    }

                    reply.bulletf("🧑‍💻 %s", user.username.c_str());
                    if (!reply.isTruncated()) {
                        reply.detailf("🛡️ Admin: %s", user.admin ? "Yes" : "No");
                    }
                    if (!reply.isTruncated()) {
                        reply.detailf("🔐 Credential: %s", user.password.isEmpty() ? "Missing" : "Stored (hidden)");
                    }
                    if (!reply.isTruncated()) {
                        reply.line();
                    }
                }
            }
        });
    }

    reply.finalize();
    return true;
}

}  // namespace TELEGRAM::Commands
