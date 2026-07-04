/**
 * @file TelegramCommandSections.h
 * @brief Shared reply sections used by multiple Telegram commands
 */

#pragma once

#include "TelegramReplyBuilder.h"

namespace TELEGRAM::Commands::Sections {

struct ExternalIpInfo {
    bool wifiConnected = false;
    bool success = false;
    uint32_t pingMs = 0;
    char ip[32] = {0};
    char error[96] = {0};
};

ExternalIpInfo fetchExternalIp();
void appendExternalIpSection(TelegramReplyBuilder& reply, const ExternalIpInfo& info, bool useHeader);
void appendHealthSection(TelegramReplyBuilder& reply, bool detailed, bool useHeader);
void appendBleSection(TelegramReplyBuilder& reply, bool detailed, bool useHeader);

}  // namespace TELEGRAM::Commands::Sections
