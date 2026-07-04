#include "GpioTypes.h"

#include <cstring>

namespace GPIO {

GpioChannelConfig::GpioChannelConfig()
    : pin(0),
      mode(GpioMode::Disabled),
      pull(GpioPull::None),
      inverted(false),
      debounceMs(kDefaultDebounceMs),
      initialOutput(false) {
    memset(id, 0, sizeof(id));
    memset(name, 0, sizeof(name));
}

const char* modeToString(GpioMode mode) {
    switch (mode) {
        case GpioMode::Disabled:
            return "disabled";
        case GpioMode::Input:
            return "input";
        case GpioMode::Output:
            return "output";
    }
    return "disabled";
}

bool stringToMode(const char* value, GpioMode& out) {
    if (!value) {
        return false;
    }
    if (strcmp(value, "disabled") == 0) {
        out = GpioMode::Disabled;
        return true;
    }
    if (strcmp(value, "input") == 0) {
        out = GpioMode::Input;
        return true;
    }
    if (strcmp(value, "output") == 0) {
        out = GpioMode::Output;
        return true;
    }
    return false;
}

const char* pullToString(GpioPull pull) {
    switch (pull) {
        case GpioPull::None:
            return "none";
        case GpioPull::Up:
            return "up";
        case GpioPull::Down:
            return "down";
    }
    return "none";
}

bool stringToPull(const char* value, GpioPull& out) {
    if (!value) {
        return false;
    }
    if (strcmp(value, "none") == 0) {
        out = GpioPull::None;
        return true;
    }
    if (strcmp(value, "up") == 0 || strcmp(value, "pullup") == 0) {
        out = GpioPull::Up;
        return true;
    }
    if (strcmp(value, "down") == 0 || strcmp(value, "pulldown") == 0) {
        out = GpioPull::Down;
        return true;
    }
    return false;
}

bool isInputMode(GpioMode mode) {
    return mode == GpioMode::Input;
}

bool isOutputMode(GpioMode mode) {
    return mode == GpioMode::Output;
}

bool copySafeId(char* dest, size_t destSize, const char* value) {
    if (!dest || destSize == 0 || !value || value[0] == '\0' || strlen(value) >= destSize) {
        return false;
    }
    strlcpy(dest, value, destSize);
    return true;
}

bool copySafeName(char* dest, size_t destSize, const char* value) {
    if (!dest || destSize == 0 || !value || value[0] == '\0' || strlen(value) >= destSize) {
        return false;
    }
    strlcpy(dest, value, destSize);
    return true;
}

}  // namespace GPIO

