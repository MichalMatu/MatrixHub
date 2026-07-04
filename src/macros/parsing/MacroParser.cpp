#include "MacroParser.h"

#if defined(ARDUINO) || defined(ESP_PLATFORM) || defined(NATIVE_BUILD)

#ifndef NATIVE_BUILD
#include <USBHIDMouse.h> // For MOUSE_LEFT, MOUSE_RIGHT, MOUSE_MIDDLE
#include <USBHIDKeyboard.h> // Dependency for key codes
#else
// Mock Key Codes for Native Build
#define KEY_LEFT_CTRL   0x80
#define KEY_LEFT_SHIFT  0x81
#define KEY_LEFT_ALT    0x82
#define KEY_LEFT_GUI    0x83
#define KEY_UP_ARROW    0xDA
#define KEY_DOWN_ARROW  0xD9
#define KEY_LEFT_ARROW  0xD8
#define KEY_RIGHT_ARROW 0xD7
#define KEY_BACKSPACE   0xB2
#define KEY_TAB         0xB3
#define KEY_RETURN      0xB0
#define KEY_ESC         0xB1
#define KEY_INSERT      0xD1
#define KEY_DELETE      0xD4
#define KEY_PAGE_UP     0xD3
#define KEY_PAGE_DOWN   0xD6
#define KEY_HOME        0xD2
#define KEY_END         0xD5
#define KEY_CAPS_LOCK   0xC1
#define KEY_F1          0xC2
#define KEY_F2          0xC3
#define KEY_F3          0xC4
#define KEY_F4          0xC5
#define KEY_F5          0xC6
#define KEY_F6          0xC7
#define KEY_F7          0xC8
#define KEY_F8          0xC9
#define KEY_F9          0xCA
#define KEY_F10         0xCB
#define KEY_F11         0xCC
#define KEY_F12         0xCD
#define KEY_F13         0xF0
#define KEY_F14         0xF1
#define KEY_F15         0xF2
#define KEY_F16         0xF3
#define KEY_F17         0xF4
#define KEY_F18         0xF5
#define KEY_F19         0xF6
#define KEY_F20         0xF7
#define KEY_F21         0xF8
#define KEY_F22         0xF9
#define KEY_F23         0xFA
#define KEY_F24         0xFB

#define MOUSE_LEFT 1
#define MOUSE_RIGHT 2
#define MOUSE_MIDDLE 4
#endif
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "MacroParser"

namespace MACROS {

// Helper: Safe number parsing with validation
static int32_t safeParseInt(const char* str, int32_t defaultValue = 0, bool allowNegative = false) {
    if (!str || *str == '\0') return defaultValue;
    char* endptr = nullptr;
    long val = strtol(str, &endptr, 10);
    if (endptr == str) {
        LOGW("Invalid number: '%s', using default %d", str, defaultValue);
        return defaultValue;
    }
    // WARN-2: Clamp negative values to 0 only if NOT allowed
    if (!allowNegative && val < 0) {
        LOGW("Negative value '%d' clamped to 0", (int)val);
        return 0;
    }
    return static_cast<int32_t>(val);
}

static uint32_t safeParseBoundedUInt(const char* str, uint32_t defaultValue, uint32_t minValue, uint32_t maxValue) {
    if (!str || *str == '\0') return defaultValue;
    char* endptr = nullptr;
    long long val = strtoll(str, &endptr, 10);
    if (endptr == str) {
        LOGW("Invalid number: '%s', using default %u", str, (unsigned)defaultValue);
        return defaultValue;
    }
    if (val < static_cast<long long>(minValue)) {
        LOGW("Value '%lld' clamped to %u", val, (unsigned)minValue);
        return minValue;
    }
    if (static_cast<unsigned long long>(val) > maxValue) {
        LOGW("Value '%lld' clamped to %u", val, (unsigned)maxValue);
        return maxValue;
    }
    return static_cast<uint32_t>(val);
}

MacroParser::MacroParser() 
    : _fileOpen(false), _memoryMode(false), _memoryData(nullptr), _lineNumber(0), _bufferIndex(0), _bufferLength(0), 
      _error(PsramAllocator<char>()), _buffer(PsramAllocator<uint8_t>()) {
    // Allocate buffer in PSRAM (via PsramVector default allocator)
    try {
        _buffer.resize(BUFFER_SIZE);
    } catch (const std::bad_alloc&) {
        LOGE("CRITICAL: Failed to allocate parser buffer in PSRAM");
        // Buffer remains empty. begin() will check and fail gracefully.
    }
}

MacroParser::~MacroParser() {
    end();
}

bool MacroParser::begin(const char* filename) {
    // Check if buffer was allocated successfully in constructor
    if (_buffer.size() < BUFFER_SIZE) {
        _error = "Parser buffer allocation failed";
        return false;
    }
    
    // Close any previously open file to prevent handle leak
    end();
    
    // Security Check BEFORE opening file
    if (String(filename).indexOf("..") != -1) {
        _error = "Security Violation: Invalid filename";
        return false;
    }
    
    if (!LittleFS.exists(filename)) {
        _error = "File not found";
        return false;
    }
    _file = LittleFS.open(filename, "r");
    if (!_file) {
        _error = "Failed to open file";
        return false;
    }

    _fileOpen = true;
    _memoryMode = false;
    _memoryData = nullptr;
    _lineNumber = 0;
    _bufferIndex = 0;
    _bufferLength = 0;
    _error.clear();
    return true;
}

bool MacroParser::beginFromContent(const uint8_t* data, size_t len) {
    end();
    
    if (!data || len == 0) {
        _error = "Empty content";
        return false;
    }
    
    _memoryData = data;
    _bufferLength = len;
    _bufferIndex = 0;
    _lineNumber = 0;
    _fileOpen = true;   // Signals that parsing is active
    _memoryMode = true; // No file handle — parse from buffer only
    _error.clear();
    return true;
}

void MacroParser::end() {
    if (_file && !_memoryMode) _file.close();
    _fileOpen = false;
    _memoryMode = false;
}

void MacroParser::fillBuffer() {
    if (!_fileOpen || _memoryMode) return; // Memory mode: all data already in buffer
    _bufferIndex = 0;
    // Block read
    _bufferLength = _file.read(_buffer.data(), BUFFER_SIZE);
}

bool MacroParser::readLine(PsramString& line) {
    line.clear();
    // Optimization: Reserve likely capacity to avoid reallocations
    line.reserve(64); 
    
    if (!_fileOpen) return false;

    bool contentRead = false;
    while ((_memoryMode ? (_bufferIndex < _bufferLength) : _file.available()) || _bufferIndex < _bufferLength) {
        
        // Refill buffer if empty
        if (!_memoryMode && _bufferIndex >= _bufferLength) {
            fillBuffer();
            if (_bufferLength == 0) break; // EOF
        }

        char c = _memoryMode ? (char)_memoryData[_bufferIndex++] : (char)_buffer[_bufferIndex++];
        
        // Handle standard line endings (CRLF or LF)
        if (c == '\r') continue;
        if (c == '\n') return true; 
        
        // Safety: Truncate excessively long lines
        if (line.length() < LIMITS::MAX_LINE_LENGTH) {
            line += c;
        } else if (line.length() == LIMITS::MAX_LINE_LENGTH) {
            // WARN-9: Log only once at truncation point
            LOGW("Line %d truncated at %zu chars", _lineNumber + 1, LIMITS::MAX_LINE_LENGTH);
        }
        contentRead = true;
    }
    return contentRead;
}

bool MacroParser::next(MacroCommand& cmd) {
    PsramString line;
    if (!readLine(line)) return false; // EOF

    _lineNumber++;
    
    // Simple trim
    size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos) {
        cmd.type = CommandType::REM; // Treat empty lines as comments
        return true;
    }
    size_t last = line.find_last_not_of(" \t");
    line = line.substr(first, (last - first + 1));

    // Check for comments
    if (line.rfind("REM", 0) == 0) {
        cmd.type = CommandType::REM;
        return true;
    }

    parseLine(line, cmd);
    return true;
}

void MacroParser::parseLine(const PsramString& line, MacroCommand& cmd) {
    // Reset command
    cmd = MacroCommand(); 

    size_t spacePos = line.find(' ');
    PsramString commandStr = (spacePos == std::string::npos) ? line : line.substr(0, spacePos);
    PsramString args = (spacePos == std::string::npos) ? "" : line.substr(spacePos + 1);

    // Convert command to upper for check
    for (auto & c: commandStr) c = toupper(c);

    // Command Dispatch Logic
    if (commandStr == "DELAY") {
        cmd.type = CommandType::DELAY;
        cmd.numericData = safeParseBoundedUInt(args.c_str(), 0, 0, LIMITS::MAX_DELAY_MS);
    } 
    else if (commandStr == "STRING") {
        cmd.type = CommandType::STRING;
        cmd.textData = args;
    }
    else if (commandStr == "STRINGLN") {
        cmd.type = CommandType::STRINGLN;
        cmd.textData = args;
    }
    else if (commandStr == "REPEAT") {
        cmd.type = CommandType::REPEAT;
        cmd.numericData = safeParseBoundedUInt(args.c_str(), 1, 1, LIMITS::MAX_REPEAT_COUNT);
    }
    else if (commandStr == "GUI" || commandStr == "WINDOWS" ||
             commandStr == "CTRL" || commandStr == "CONTROL" ||
             commandStr == "SHIFT" || commandStr == "ALT") {
        // Multi-modifier combo parsing (e.g., "CTRL ALT DELETE", "GUI SHIFT s")
        cmd.type = CommandType::COMBO;
        cmd.modifiers = 0;
        cmd.key = 0;
        
        // Start with the first token as a modifier
        PsramString token = commandStr;
        PsramString remaining = args;
        
        while (true) {
            // Check if current token is a modifier (case-insensitive)
            PsramString upperToken = token;
            for (auto& c : upperToken) c = toupper(c);
            
            uint8_t mod = 0;
            if (upperToken == "CTRL" || upperToken == "CONTROL") mod = MOD_CTRL;
            else if (upperToken == "SHIFT") mod = MOD_SHIFT;
            else if (upperToken == "ALT") mod = MOD_ALT;
            else if (upperToken == "GUI" || upperToken == "WINDOWS") mod = MOD_GUI;
            
            if (mod != 0) {
                cmd.modifiers |= mod;
                
                // Get next token (preserve original case!)
                if (remaining.empty()) break;

                size_t nextWord = remaining.find_first_not_of(" \t");
                if (nextWord == std::string::npos) {
                    remaining.clear();
                    break;
                }
                if (nextWord > 0) {
                    remaining = remaining.substr(nextWord);
                }

                size_t sp = remaining.find(' ');
                if (sp == std::string::npos) {
                    token = remaining;
                    remaining.clear();
                } else {
                    token = remaining.substr(0, sp);
                    remaining = remaining.substr(sp + 1);
                    size_t nextWordAfter = remaining.find_first_not_of(" \t");
                    if (nextWordAfter != std::string::npos) {
                        remaining = remaining.substr(nextWordAfter);
                    } else {
                        remaining.clear();
                    }
                }
            } else {
                // Not a modifier - this is the final key
                // IMPORTANT: pass original case to parseKey!
                // Uppercase 'Z' (0x5A) causes Arduino HID to add implicit Shift,
                // so 'GUI z' would become Cmd+Shift+Z (Redo) instead of Cmd+Z (Undo).
                cmd.key = parseKey(token);
                break;
            }
        }
    }
    else if (commandStr == "DEFAULT_DELAY" || commandStr == "DEFAULTDELAY") {
        cmd.type = CommandType::DEFAULT_DELAY;
        cmd.numericData = safeParseBoundedUInt(args.c_str(), 0, 0, LIMITS::MAX_DEFAULT_DELAY_MS);
    }
    else if (commandStr == "MOUSE_MOVE") {
        cmd.type = CommandType::MOUSE_MOVE;
        // Parse x y
        int x = 0, y = 0;
        size_t split = args.find(' ');
        if (split != std::string::npos) {
            x = safeParseInt(args.substr(0, split).c_str(), 0, true);
            y = safeParseInt(args.substr(split + 1).c_str(), 0, true);
        }
        
        // Clamp to int16 range to prevent wrapping artifacts
        if (x > 32767) x = 32767;
        if (x < -32768) x = -32768;
        if (y > 32767) y = 32767;
        if (y < -32768) y = -32768;

        cmd.numericData = (uint32_t)((int16_t)x << 16 | ((int16_t)y & 0xFFFF));
    }
    else if (commandStr == "MOUSE_CLICK") {
        cmd.type = CommandType::MOUSE_CLICK;
        // LEFT, RIGHT, MIDDLE
        PsramString btnStr = args; 
        for (auto & c: btnStr) c = toupper(c);
        if (btnStr == "LEFT") cmd.key = MOUSE_LEFT;
        else if (btnStr == "RIGHT") cmd.key = MOUSE_RIGHT;
        else if (btnStr == "MIDDLE") cmd.key = MOUSE_MIDDLE;
        else {
             // Invalid button - Log error and do not set command
             LOGW("Invalid MOUSE_CLICK button: %s", args.c_str());
             cmd.type = CommandType::UNKNOWN; 
             cmd.textData = "Invalid MOUSE_CLICK button";
             cmd.key = 0;
        }
    }
    else if (commandStr == "MATRIX_PRINT") {
        cmd.type = CommandType::MATRIX_PRINT;
        cmd.textData = args;
        // Strip quotes if present
        if (cmd.textData.length() >= 2 && cmd.textData.front() == '"' && cmd.textData.back() == '"') {
             cmd.textData = cmd.textData.substr(1, cmd.textData.length() - 2);
        }
    }
    else if (commandStr == "SYSTEM_SLEEP") {
        cmd.type = CommandType::SYSTEM_CONTROL;
        cmd.key = 0x02; // SYSTEM_CONTROL_STANDBY (sleep)
    }
    else if (commandStr == "SYSTEM_WAKE") {
        cmd.type = CommandType::SYSTEM_CONTROL;
        cmd.key = 0x03; // SYSTEM_CONTROL_WAKE_HOST
    }
    else if (commandStr == "SYSTEM_POWER_OFF") {
        cmd.type = CommandType::SYSTEM_CONTROL;
        cmd.key = 0x01; // SYSTEM_CONTROL_POWER_OFF
    }
    else if (commandStr == "REPLAY") {
         cmd.type = CommandType::REPEAT;
         cmd.numericData = safeParseBoundedUInt(args.c_str(), 1, 1, LIMITS::MAX_REPEAT_COUNT);
    }
    else if (commandStr == "CONSUMER" || commandStr == "MEDIA") {
        cmd.type = CommandType::CONSUMER;
        // Parse usage code (hex or named)
        // Check if starts with 0x for hex parsing
        if (args.length() > 2 && args.substr(0, 2) == "0x") {
            cmd.key = (uint16_t)strtol(args.c_str(), nullptr, 16);
        } else {
            // Named consumer keys
            String upperArgs = args.c_str();
            upperArgs.toUpperCase();
            if (upperArgs == "VOLUME_UP") cmd.key = 0xE9;
            else if (upperArgs == "VOLUME_DOWN") cmd.key = 0xEA;
            else if (upperArgs == "MUTE") cmd.key = 0xE2;
            else if (upperArgs == "PLAY" || upperArgs == "PAUSE" || upperArgs == "PLAY_PAUSE") cmd.key = 0xCD;
            else if (upperArgs == "NEXT" || upperArgs == "NEXT_TRACK") cmd.key = 0xB5;
            else if (upperArgs == "PREV" || upperArgs == "PREV_TRACK") cmd.key = 0xB6;
            else if (upperArgs == "STOP") cmd.key = 0xB7;
            else if (upperArgs == "EJECT") cmd.key = 0xB8;
            // Add more named keys as needed
            else cmd.key = 0; // Unknown
        }
    }
    else if (commandStr == "KEY") {
        cmd.type = CommandType::KEY;
        // Check for hex code (KEY 0x28)
        if (args.length() > 2 && args.substr(0, 2) == "0x") {
            cmd.key = (uint8_t)strtol(args.c_str(), nullptr, 16);
        } else {
            // Parse named key from arguments
            cmd.key = parseKey(args);
        }
    }
    else if (commandStr == "PRESS") {
        cmd.type = CommandType::PRESS_KEY;
        // Check for hex code
        if (args.length() > 2 && args.substr(0, 2) == "0x") {
            cmd.key = (uint8_t)strtol(args.c_str(), nullptr, 16);
        } else {
            cmd.key = parseKey(args);
        }
    }
    else if (commandStr == "RELEASE") {
        PsramString argUpper = args;
        for (auto & c: argUpper) c = toupper(c);
        
        if (argUpper == "ALL") {
            cmd.type = CommandType::RELEASE_ALL;
        } else {
            cmd.type = CommandType::RELEASE_KEY;
            // Check for hex code
            if (args.length() > 2 && args.substr(0, 2) == "0x") {
                cmd.key = (uint8_t)strtol(args.c_str(), nullptr, 16);
            } else {
                cmd.key = parseKey(args);
            }
        }
    }
    else {
        // Implicit key press (e.g., "ENTER")
        uint8_t k = parseKey(commandStr);
        if (k != 0) {
            cmd.type = CommandType::KEY;
            cmd.key = k;
        } else {
            // Unknown command fallback
            cmd.type = CommandType::UNKNOWN;
            cmd.textData = "UNKNOWN: " + line;
        }
    }
}

uint8_t MacroParser::parseKey(const PsramString& keyStr) {
    if (keyStr.length() == 1) return (uint8_t)keyStr[0];
    
    PsramString upper = keyStr;
    for (auto & c: upper) c = toupper(c);

    struct KeyMap {
        const char* name;
        uint8_t code;
    };

    static const KeyMap lookup[] = {
        {"ENTER", KEY_RETURN},
        {"ESC", KEY_ESC},
        {"ESCAPE", KEY_ESC},
        {"BACKSPACE", KEY_BACKSPACE},
        {"TAB", KEY_TAB},
        {"SPACE", ' '},
        {"CAPSLOCK", KEY_CAPS_LOCK},
        {"INSERT", KEY_INSERT},
        {"DELETE", KEY_DELETE},
        {"DEL", KEY_DELETE},
        {"HOME", KEY_HOME},
        {"END", KEY_END},
        {"PAGEUP", KEY_PAGE_UP},
        {"PAGE_UP", KEY_PAGE_UP},
        {"PAGEDOWN", KEY_PAGE_DOWN},
        {"PAGE_DOWN", KEY_PAGE_DOWN},
        {"UP", KEY_UP_ARROW},
        {"DOWN", KEY_DOWN_ARROW},
        {"LEFT", KEY_LEFT_ARROW},
        {"RIGHT", KEY_RIGHT_ARROW},
        {"GUI", KEY_LEFT_GUI},
        {"WINDOWS", KEY_LEFT_GUI},
        {"CTRL", KEY_LEFT_CTRL},
        {"CONTROL", KEY_LEFT_CTRL},
        {"SHIFT", KEY_LEFT_SHIFT},
        {"ALT", KEY_LEFT_ALT},
        {"F1", KEY_F1}, {"F2", KEY_F2}, {"F3", KEY_F3}, {"F4", KEY_F4},
        {"F5", KEY_F5}, {"F6", KEY_F6}, {"F7", KEY_F7}, {"F8", KEY_F8},
        {"F9", KEY_F9}, {"F10", KEY_F10}, {"F11", KEY_F11}, {"F12", KEY_F12}
    };

    for (const auto& item : lookup) {
        if (upper == item.name) return item.code;
    }

    return 0;
}

uint8_t MacroParser::getModifier(const PsramString& modName) {
    // Helper not strictly needed if parsed inline, providing for completeness
    PsramString upper = modName;
    for (auto & c: upper) c = toupper(c);
    
    if (upper == "CTRL" || upper == "CONTROL") return KEY_LEFT_CTRL;
    if (upper == "SHIFT") return KEY_LEFT_SHIFT;
    if (upper == "ALT") return KEY_LEFT_ALT;
    if (upper == "GUI" || upper == "WINDOWS") return KEY_LEFT_GUI;
    return 0;
}

} // namespace MACROS

#endif // ARDUINO || ESP_PLATFORM || NATIVE_BUILD
