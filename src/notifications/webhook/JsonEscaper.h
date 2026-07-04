/**
 * @file JsonEscaper.h
 * @brief JSON string escaping utility
 * 
 * Provides proper JSON string escaping for webhook payloads.
 * Handles all control characters per RFC 8259.
 */

#pragma once

#include <cstddef>
#include <Arduino.h>

namespace NOTIFICATIONS {

/**
 * JSON string escaper utility.
 * 
 * Escapes special characters according to JSON specification:
 * - Quotation mark (") -> \"
 * - Reverse solidus (\) -> \\
 * - Newline (0x0A) -> \n
 * - Carriage return (0x0D) -> \r
 * - Tab (0x09) -> \t
 * - Backspace (0x08) -> \b
 * - Form feed (0x0C) -> \f
 * - Other control chars (0x00-0x1F) -> skipped
 */
class JsonEscaper {
public:
    /**
     * Escape a string for use in JSON value.
     * 
     * @param input Source string to escape
     * @param inputLen Length of source string
     * @param output Output buffer for escaped string (without quotes)
     * @param outputSize Size of output buffer
     * @return Number of characters written (excluding null terminator)
     * 
     * Note: Does NOT add surrounding quotes - caller is responsible for JSON structure.
     */
    static size_t escape(
        const char* input, 
        size_t inputLen, 
        char* output, 
        size_t outputSize
    );

    /**
     * Get the length of the string after escaping.
     * 
     * @param input Source string
     * @return Escaped string length
     */
    static size_t getEscapedLength(const char* input);

    /**
     * Wrap text in a simple JSON object: {"text":"<escaped_message>"}
     * 
     * @param message Source message
     * @param messageLen Length of message
     * @param output Output buffer for complete JSON
     * @param outputSize Size of output buffer
     * @return Number of characters written (excluding null terminator), 0 on error
     */
    static size_t wrapAsTextJson(
        const char* message,
        size_t messageLen,
        char* output,
        size_t outputSize
    );

    /**
     * Wrap text in a JSON object with a caller-provided key:
     * {"<key>":"<escaped_message>"}
     *
     * @param key JSON field name (e.g. "text", "content")
     * @param message Source message
     * @param messageLen Length of message
     * @param output Output buffer for complete JSON
     * @param outputSize Size of output buffer
     * @return Number of characters written (excluding null terminator), 0 on error
     */
    static size_t wrapAsKeyJson(
        const char* key,
        const char* message,
        size_t messageLen,
        char* output,
        size_t outputSize
    );

private:
    JsonEscaper() = delete; // Static-only class
};

}  // namespace NOTIFICATIONS
