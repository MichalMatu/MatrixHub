/**
 * @file TelegramReplyBuilder.cpp
 * @brief Shared fixed-buffer reply builder for Telegram command handlers
 */

#include "TelegramReplyBuilder.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace TELEGRAM::Commands {

namespace {
constexpr const char* kTruncatedSuffix = "\n[truncated]";
}

TelegramReplyBuilder::TelegramReplyBuilder(CommandContext& ctx) : _ctx(ctx) {
    _ctx.response[0] = '\0';
    _ctx.responseLen = 0;
    _ctx.shouldReply = false;
}

void TelegramReplyBuilder::appendRaw(const char* text, size_t len) {
    if (!text || len == 0 || _truncated) {
        return;
    }

    const size_t capacity = sizeof(_ctx.response);
    if (capacity == 0 || _len >= capacity - 1) {
        _truncated = true;
        return;
    }

    const size_t available = capacity - 1 - _len;
    if (len > available) {
        memcpy(_ctx.response + _len, text, available);
        _len += available;
        _ctx.response[_len] = '\0';
        _truncated = true;
        return;
    }

    memcpy(_ctx.response + _len, text, len);
    _len += len;
    _ctx.response[_len] = '\0';
}

void TelegramReplyBuilder::appendV(const char* fmt, va_list args) {
    if (!fmt || _truncated) {
        return;
    }

    const size_t capacity = sizeof(_ctx.response);
    if (capacity == 0 || _len >= capacity - 1) {
        _truncated = true;
        return;
    }

    const size_t remaining = capacity - _len;
    const int written = vsnprintf(_ctx.response + _len, remaining, fmt, args);
    if (written <= 0) {
        return;
    }

    if (static_cast<size_t>(written) >= remaining) {
        _len = capacity - 1;
        _ctx.response[_len] = '\0';
        _truncated = true;
        return;
    }

    _len += static_cast<size_t>(written);
}

void TelegramReplyBuilder::header(const char* icon, const char* title) {
    if (icon && icon[0]) {
        linef("%s %s", icon, title ? title : "");
    } else {
        linef("%s", title ? title : "");
    }
    line();
}

void TelegramReplyBuilder::section(const char* icon, const char* title) {
    if (icon && icon[0]) {
        linef("%s %s", icon, title ? title : "");
    } else {
        linef("%s", title ? title : "");
    }
}

void TelegramReplyBuilder::line() {
    appendRaw("\n", 1);
}

void TelegramReplyBuilder::line(const char* text) {
    if (text && text[0]) {
        appendRaw(text, strlen(text));
    }
    appendRaw("\n", 1);
}

void TelegramReplyBuilder::linef(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    appendV(fmt, args);
    va_end(args);
    appendRaw("\n", 1);
}

void TelegramReplyBuilder::detailf(const char* fmt, ...) {
    appendRaw("  ", 2);
    va_list args;
    va_start(args, fmt);
    appendV(fmt, args);
    va_end(args);
    appendRaw("\n", 1);
}

void TelegramReplyBuilder::bulletf(const char* fmt, ...) {
    appendRaw("- ", 2);
    va_list args;
    va_start(args, fmt);
    appendV(fmt, args);
    va_end(args);
    appendRaw("\n", 1);
}

void TelegramReplyBuilder::kvf(const char* icon, const char* label, const char* fmt, ...) {
    if (icon && icon[0]) {
        appendRaw(icon, strlen(icon));
        appendRaw(" ", 1);
    }
    if (label && label[0]) {
        appendRaw(label, strlen(label));
        appendRaw(": ", 2);
    }
    va_list args;
    va_start(args, fmt);
    appendV(fmt, args);
    va_end(args);
    appendRaw("\n", 1);
}

void TelegramReplyBuilder::usage(const char* usage, const char* hint) {
    linef("Usage: %s", usage ? usage : "");
    if (hint && hint[0]) {
        linef("Hint: %s", hint);
    }
}

void TelegramReplyBuilder::ensureTruncatedSuffix() {
    if (!_truncated) {
        return;
    }

    const size_t capacity = sizeof(_ctx.response);
    if (capacity <= 1) {
        return;
    }

    const size_t suffixLen = strlen(kTruncatedSuffix);
    const size_t maxLen = capacity - 1;

    if (_len + suffixLen <= maxLen) {
        memcpy(_ctx.response + _len, kTruncatedSuffix, suffixLen + 1);
        _len += suffixLen;
        return;
    }

    if (suffixLen >= maxLen) {
        memcpy(_ctx.response, kTruncatedSuffix + suffixLen - maxLen, maxLen);
        _ctx.response[maxLen] = '\0';
        _len = maxLen;
        return;
    }

    const size_t writePos = maxLen - suffixLen;
    memcpy(_ctx.response + writePos, kTruncatedSuffix, suffixLen + 1);
    _len = writePos + suffixLen;
}

void TelegramReplyBuilder::finalize(bool shouldReply) {
    ensureTruncatedSuffix();
    _ctx.responseLen = _len;
    _ctx.shouldReply = shouldReply && _len > 0;
}

}  // namespace TELEGRAM::Commands
