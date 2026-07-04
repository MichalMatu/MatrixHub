/**
 * @file MatrixCommand.h
 * @brief /matrix command handler
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * Handle /matrix command
 * Sends the provided text argument to the Matrix display queue
 */
bool handleMatrix(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
