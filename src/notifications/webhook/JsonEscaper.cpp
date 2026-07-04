
#include "JsonEscaper.h"
#include <cstring>
#include <Arduino.h>

namespace NOTIFICATIONS {

size_t JsonEscaper::escape(
    const char* input, 
    size_t inputLen, 
    char* output, 
    size_t outputSize
) {
    if (!input || !output || outputSize == 0) {
        return 0;
    }

    size_t inPos = 0;
    size_t outPos = 0;

    // Reserve 1 byte for null terminator
    size_t maxLen = outputSize - 1;

    while (inPos < inputLen && outPos < maxLen) {
        char c = input[inPos++];

        // Check for control characters that need escaping
        const char* escaped = nullptr;
        char tempEsc[2] = {0, 0}; // For single char escaping like \n

        switch (c) {
            case '\"': escaped = "\\\""; break;
            case '\\': escaped = "\\\\"; break;
            case '\b': escaped = "\\b"; break;
            case '\f': escaped = "\\f"; break;
            case '\n': escaped = "\\n"; break;
            case '\r': escaped = "\\r"; break;
            case '\t': escaped = "\\t"; break;
            default:
                if (c >= 0x00 && c <= 0x1F) {
                    // Skip other control chars
                    continue; 
                }
                break;
        }

        if (escaped) {
            size_t len = strlen(escaped);
            if (outPos + len <= maxLen) {
                strcpy(output + outPos, escaped);
                outPos += len;
            } else {
                break; // Output buffer full
            }
        } else {
            // Normal character
            output[outPos++] = c;
        }
    }

    output[outPos] = '\0';
    return outPos;
}

size_t JsonEscaper::wrapAsTextJson(
    const char* message,
    size_t messageLen,
    char* output,
    size_t outputSize
) {
    return wrapAsKeyJson("text", message, messageLen, output, outputSize);
}

size_t JsonEscaper::wrapAsKeyJson(
    const char* key,
    const char* message,
    size_t messageLen,
    char* output,
    size_t outputSize
) {
    if (!key || !message || !output || outputSize == 0) return 0;

    size_t keyLen = strlen(key);
    if (keyLen == 0) return 0;

    // Minimal JSON: {"<key>":""}
    const size_t kMinSize = 2 /* {" */ + keyLen + 3 /* ":" */ + 2 /* "" */ + 1 /* } */ + 1 /* \0 */;
    if (outputSize < kMinSize) return 0;

    size_t pos = 0;
    output[pos++] = '{';
    output[pos++] = '"';
    memcpy(output + pos, key, keyLen);
    pos += keyLen;
    output[pos++] = '"';
    output[pos++] = ':';
    output[pos++] = '"';

    // Escape message content
    // Available space: outputSize - pos - 2 (for "\"}") - 1 (null)
    size_t available = outputSize - pos - 3;
    size_t written = escape(message, messageLen, output + pos, available);
    pos += written;

    // Close JSON
    if (pos + 2 < outputSize) {
        output[pos++] = '"';
        output[pos++] = '}';
        output[pos] = '\0';
        return pos;
    }

    return 0;
}

size_t JsonEscaper::getEscapedLength(const char* input) {
    if (!input) return 0;
    
    size_t len = strlen(input);
    size_t escapedLen = 0;
    
    for (size_t i = 0; i < len; i++) {
        char c = input[i];
         switch (c) {
            case '\"': case '\\': case '\b': case '\f': case '\n': case '\r': case '\t':
                escapedLen += 2;
                break;
            default:
                if (!(c >= 0x00 && c <= 0x1F)) {
                    escapedLen += 1;
                }
                break;
        }
    }
    
    return escapedLen;
}

} // namespace NOTIFICATIONS
