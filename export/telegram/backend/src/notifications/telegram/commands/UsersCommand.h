/**
 * @file UsersCommand.h
 * @brief Implementation of /users command
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /users command - returns configured users without exposing secrets
 */
bool handleUsers(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
