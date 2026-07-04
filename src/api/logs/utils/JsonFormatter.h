#pragma once

#include <cstdint>
#include <cstddef>

namespace API {
namespace Utils {

/**
 * @brief Fast uint32_t to string conversion, returns number of chars written 
 * (hardware friendly alternative to snprintf %u)
 */
size_t fast_u32_to_str(char* out, uint32_t val);

/**
 * @brief Escapes JSON string content (no surrounding quotes)
 * 
 * @param out Output buffer where escaped string will be written
 * @param maxLen Maximum length of output buffer
 * @param s Source string to escape
 * @return size_t Number of characters written to the output buffer
 */
size_t escapeJsonString(char* out, size_t maxLen, const char* s);

} // namespace Utils
} // namespace API
