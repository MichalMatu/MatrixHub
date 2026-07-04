#pragma once

#include <Arduino.h>
#include <cstdint>
#include <cstddef>

namespace GPIO {

constexpr uint8_t kMaxChannels = 8;
constexpr uint8_t kMaxIdLen = 16;
constexpr uint8_t kMaxNameLen = 24;
constexpr uint16_t kDefaultDebounceMs = 50;
constexpr uint16_t kMinDebounceMs = 0;
constexpr uint16_t kMaxDebounceMs = 5000;
constexpr uint32_t kPollIntervalMs = 20;

enum class GpioMode : uint8_t {
    Disabled = 0,
    Input = 1,
    Output = 2,
};

enum class GpioPull : uint8_t {
    None = 0,
    Up = 1,
    Down = 2,
};

struct __attribute__((packed)) GpioChannelConfig {
    char id[kMaxIdLen];
    char name[kMaxNameLen];
    uint8_t pin;
    GpioMode mode;
    GpioPull pull;
    bool inverted;
    uint16_t debounceMs;
    bool initialOutput;

    GpioChannelConfig();
};

struct __attribute__((packed)) GpioData {
    GpioChannelConfig channels[kMaxChannels];
    uint8_t channelCount = 0;
};

struct GpioPinDefinition {
    uint8_t pin;
    const char* id;
    const char* name;
    bool allowInput;
    bool allowOutput;
    bool allowPullup;
    bool allowPulldown;
    const char* reason;
};

struct GpioChannelStatus {
    GpioChannelConfig config;
    bool configured = false;
    bool rawLevel = false;
    bool logicalLevel = false;
    bool stable = false;
    uint32_t changedAtMs = 0;
    uint32_t sampledAtMs = 0;
};

const char* modeToString(GpioMode mode);
bool stringToMode(const char* value, GpioMode& out);
const char* pullToString(GpioPull pull);
bool stringToPull(const char* value, GpioPull& out);
bool isInputMode(GpioMode mode);
bool isOutputMode(GpioMode mode);
bool copySafeId(char* dest, size_t destSize, const char* value);
bool copySafeName(char* dest, size_t destSize, const char* value);

}  // namespace GPIO

