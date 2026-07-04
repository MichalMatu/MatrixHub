/**
 * @file HealthCommand.h
 * @brief Header for /health command
 */

#pragma once

#include "TelegramCommandTypes.h"

namespace TELEGRAM::Commands {

/**
 * @brief Handle /health command
 *
 * Shows deep system diagnostics and telemetry from RTC memory.
 *
 * @param ctx Command context with response buffer
 * @return true if handled, false otherwise
 */
bool handleHealth(CommandContext& ctx);

}  // namespace TELEGRAM::Commands
