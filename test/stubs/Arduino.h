#pragma once
#include <stdint.h>
#include <cstring>
#include <string>
#include <cmath>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include "esp_system.h"
#include "esp_attr.h"
#define PROGMEM
#ifndef configASSERT
#define configASSERT(x) do { if(!(x)) {} } while(0)
#endif

// Basic Arduino types
typedef bool boolean;
typedef uint8_t byte;
typedef void* SemaphoreHandle_t;
#define pdTRUE 1
#define pdFALSE 0
#define portMAX_DELAY 0xFFFFFFFF
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) (x)
#endif

#include "freertos/FreeRTOS.h"

#ifndef INPUT
#define INPUT 0x0
#endif
#ifndef OUTPUT
#define OUTPUT 0x1
#endif
#ifndef INPUT_PULLUP
#define INPUT_PULLUP 0x2
#endif
#ifndef LOW
#define LOW 0
#endif
#ifndef HIGH
#define HIGH 1
#endif
#ifndef HEX
#define HEX 16
#endif

extern "C" {
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
void delayMicroseconds(uint32_t us);
}

// Time functions
namespace TEST_STUBS::ARDUINO {
inline uint32_t millisValue = 0;
}

namespace TEST_STUBS::SERIAL {
inline int beginCalls = 0;
inline int endCalls = 0;
inline int flushCalls = 0;
inline std::string written;

inline void reset() {
    beginCalls = 0;
    endCalls = 0;
    flushCalls = 0;
    written.clear();
}
}

inline uint32_t millis() { return TEST_STUBS::ARDUINO::millisValue; }
inline uint32_t micros() { return TEST_STUBS::ARDUINO::millisValue * 1000; }
// Notification mocks
inline void xTaskNotifyGive(TaskHandle_t xTaskToNotify) {}
inline uint32_t ulTaskNotifyTake(BaseType_t xClearCountOnExit, TickType_t xTicksToWait) { return 1; }
inline void delay(uint32_t ms) { (void)ms; }
inline long random(long maxValue) {
    if (maxValue <= 0) {
        return 0;
    }
    return std::rand() % maxValue;
}

// Math
using std::abs;
using std::max;
using std::min;
using std::round;
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) ((x)*(x))

// Mock String class
class String : public std::string {
public:
    String() : std::string() {}
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    String(int i) : std::string(std::to_string(i)) {}
    String(float f) : std::string(std::to_string(f)) {}
    String(long value, int base) {
        if (base == HEX) {
            char buffer[32];
            std::snprintf(buffer, sizeof(buffer), "%lx", value);
            assign(buffer);
        } else {
            assign(std::to_string(value));
        }
    }
    
    // Minimal Arduino String API
    unsigned char charAt(unsigned int index) const {
        return operator[](index);
    }
    
    int toInt() const {
        const char* text = c_str();
        if (!text) {
            return 0;
        }

        char* end = nullptr;
        const long value = std::strtol(text, &end, 10);
        return (end == text) ? 0 : static_cast<int>(value);
    }
    int indexOf(const char* str) const {
        size_t pos = this->find(str);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }
    int indexOf(const String& str) const {
        size_t pos = this->find(str);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    int indexOf(char c) const {
        size_t pos = this->find(c);
        return (pos == std::string::npos) ? -1 : (int)pos;
    }
    int indexOf(char c, unsigned int fromIndex) const {
        if (fromIndex >= length()) return -1;
        size_t pos = this->find(c, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    int indexOf(const char* str, unsigned int fromIndex) const {
        if (str == nullptr || fromIndex >= length()) return -1;
        size_t pos = this->find(str, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    int indexOf(const String& str, unsigned int fromIndex) const {
        if (fromIndex >= length()) return -1;
        size_t pos = this->find(str, fromIndex);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }
    void toUpperCase() {
        for (auto & c: *this) c = toupper(c);
    }
    bool startsWith(const char* prefix) const {
        if (prefix == nullptr) return false;
        size_t prefixLen = strlen(prefix);
        if (prefixLen > length()) return false;
        return compare(0, prefixLen, prefix) == 0;
    }
    bool startsWith(const String& prefix) const {
        return startsWith(prefix.c_str());
    }
    bool endsWith(const char* suffix) const {
        if (suffix == nullptr) return false;
        size_t suffixLen = strlen(suffix);
        if (suffixLen > length()) return false;
        return compare(length() - suffixLen, suffixLen, suffix) == 0;
    }
    bool endsWith(const String& suffix) const {
        return endsWith(suffix.c_str());
    }
    bool isEmpty() const { return empty(); }

    String substring(unsigned int beginIndex) const {
        if (beginIndex >= length()) return String();
        return String(std::string::substr(beginIndex));
    }
    String substring(unsigned int beginIndex, unsigned int endIndex) const {
        if (beginIndex >= length() || beginIndex >= endIndex) return String();
        endIndex = std::min<unsigned int>(endIndex, static_cast<unsigned int>(length()));
        return String(std::string::substr(beginIndex, endIndex - beginIndex));
    }
    void trim() {
        size_t start = 0;
        while (start < length() && std::isspace(static_cast<unsigned char>((*this)[start]))) {
            ++start;
        }
        size_t end = length();
        while (end > start && std::isspace(static_cast<unsigned char>((*this)[end - 1]))) {
            --end;
        }
        *this = substring(start, end);
    }
    void remove(unsigned int index) {
        if (index < length()) {
            erase(index);
        }
    }
    void remove(unsigned int index, unsigned int count) {
        if (index < length()) {
            erase(index, count);
        }
    }
    int lastIndexOf(char c) const {
        size_t pos = this->rfind(c);
        return (pos == std::string::npos) ? -1 : static_cast<int>(pos);
    }

    // Arduino String::replace(from, to) — replaces all occurrences
    void replace(const String& from, const String& to) {
        if (from.empty()) return;
        size_t pos = 0;
        while ((pos = find(from, pos)) != std::string::npos) {
            std::string::replace(pos, from.length(), to);
            pos += to.length();
        }
    }
    void replace(const char* from, const char* to) {
        replace(String(from), String(to));
    }
    
    bool equalsIgnoreCase(const String& other) const {
        if (length() != other.length()) return false;
        for (size_t i = 0; i < length(); i++) {
               if (tolower(at(i)) != tolower(other.at(i))) return false;
        }
        return true;
    }
    
    // Comparison with C string
    bool operator==(const char* other) const {
        return strcmp(c_str(), other) == 0;
    }
    
    bool operator!=(const char* other) const {
        return !(*this == other);
    }

    size_t write(uint8_t c) {
        push_back(static_cast<char>(c));
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) {
        if (!buffer || size == 0) {
            return 0;
        }
        append(reinterpret_cast<const char*>(buffer), size);
        return size;
    }
};

// Print Mock
class Print {
public:
    virtual size_t write(uint8_t) = 0;
    size_t write(const char *str) {
        if (str == NULL) return 0;
        return write((const uint8_t *)str, strlen(str));
    }
    size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            if (write(*buffer++)) n++;
            else break;
        }
        return n;
    }
    size_t print(const char* s) { return write(s); }
    size_t print(const String& s) { return write(s.c_str()); }
    // ...
};

// Stream Mock
class Stream : public Print {
public:
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    // ...
};

// Serial mock
class SerialClass : public Stream {
public:
    void begin(unsigned long = 0) { TEST_STUBS::SERIAL::beginCalls++; }
    void end() { TEST_STUBS::SERIAL::endCalls++; }
    void print(const char* s) {
        if (!s) {
            return;
        }
        (void)Print::write(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
    }
    void print(int n) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%d", n);
        (void)Print::write(reinterpret_cast<const uint8_t*>(buffer), std::strlen(buffer));
    }
    void println(const char* s) {
        print(s);
        (void)write('\n');
    }
    void println() { (void)write('\n'); }
    explicit operator bool() const { return true; }
    size_t availableForWrite() const { return static_cast<size_t>(-1); }
    
    // Stream impl
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override { TEST_STUBS::SERIAL::flushCalls++; }
    size_t write(uint8_t c) override {
        TEST_STUBS::SERIAL::written.push_back(static_cast<char>(c));
        return 1;
    }
};
inline SerialClass Serial;
