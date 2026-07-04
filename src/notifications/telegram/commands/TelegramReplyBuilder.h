/**
 * @file TelegramReplyBuilder.h
 * @brief Shared fixed-buffer reply builder for Telegram command handlers
 */

#pragma once

#include "TelegramCommandTypes.h"

#include <cstdarg>
#include <cstddef>

namespace TELEGRAM::Commands {

class TelegramReplyBuilder {
public:
    explicit TelegramReplyBuilder(CommandContext& ctx);

    bool isTruncated() const { return _truncated; }
    size_t length() const { return _len; }

    void header(const char* icon, const char* title);
    void section(const char* icon, const char* title);
    void line();
    void line(const char* text);
    void linef(const char* fmt, ...);
    void detailf(const char* fmt, ...);
    void bulletf(const char* fmt, ...);
    void kvf(const char* icon, const char* label, const char* fmt, ...);
    void usage(const char* usage, const char* hint = nullptr);
    void finalize(bool shouldReply = true);

private:
    CommandContext& _ctx;
    size_t _len = 0;
    bool _truncated = false;

    void appendRaw(const char* text, size_t len);
    void appendV(const char* fmt, va_list args);
    void ensureTruncatedSuffix();
};

}  // namespace TELEGRAM::Commands
