#include "GpioSafePins.h"

#include <cstring>

namespace GPIO {
namespace {

constexpr GpioPinDefinition kAllowedPins[] = {
    {1, "gpio1", "GPIO 1", true, true, true, true, "Safe exposed header GPIO"},
    {2, "gpio2", "GPIO 2", true, true, true, true, "Safe exposed header GPIO"},
    {4, "gpio4", "GPIO 4", true, true, true, true, "Safe exposed header GPIO"},
    {5, "gpio5", "GPIO 5", true, true, true, true, "Safe exposed header GPIO"},
    {38, "gpio38", "GPIO 38", true, true, true, true, "Safe exposed header GPIO"},
    {39, "gpio39", "GPIO 39", true, true, true, true, "Safe exposed header GPIO"},
    {40, "gpio40", "GPIO 40", true, true, true, true, "Safe exposed header GPIO"},
};

constexpr uint8_t kAllowedPinCount = sizeof(kAllowedPins) / sizeof(kAllowedPins[0]);

void fillDefaultChannel(GpioChannelConfig& channel, const GpioPinDefinition& pin) {
    channel = GpioChannelConfig();
    strlcpy(channel.id, pin.id, sizeof(channel.id));
    strlcpy(channel.name, pin.name, sizeof(channel.name));
    channel.pin = pin.pin;
    channel.mode = GpioMode::Disabled;
    channel.pull = GpioPull::None;
    channel.inverted = false;
    channel.debounceMs = kDefaultDebounceMs;
    channel.initialOutput = false;
}

bool hasDuplicatePinOrId(const GpioData& data, uint8_t index) {
    const auto& channel = data.channels[index];
    for (uint8_t i = 0; i < index; i++) {
        if (data.channels[i].pin == channel.pin ||
            strncmp(data.channels[i].id, channel.id, kMaxIdLen) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

const GpioPinDefinition* allowedPins() {
    return kAllowedPins;
}

uint8_t allowedPinCount() {
    return kAllowedPinCount;
}

const GpioPinDefinition* findAllowedPin(uint8_t pin) {
    for (const auto& item : kAllowedPins) {
        if (item.pin == pin) {
            return &item;
        }
    }
    return nullptr;
}

const GpioPinDefinition* findAllowedPinById(const char* id) {
    if (!id) {
        return nullptr;
    }
    for (const auto& item : kAllowedPins) {
        if (strcmp(item.id, id) == 0) {
            return &item;
        }
    }
    return nullptr;
}

bool isPinAllowed(uint8_t pin) {
    return findAllowedPin(pin) != nullptr;
}

void applyDefaultConfig(GpioData& data) {
    data = GpioData{};
    data.channelCount = kAllowedPinCount;
    for (uint8_t i = 0; i < kAllowedPinCount && i < kMaxChannels; i++) {
        fillDefaultChannel(data.channels[i], kAllowedPins[i]);
    }
}

bool validateChannelConfig(const GpioChannelConfig& channel) {
    const GpioPinDefinition* pin = findAllowedPinById(channel.id);
    if (!pin || pin->pin != channel.pin) {
        return false;
    }
    if (channel.name[0] == '\0') {
        return false;
    }
    if (channel.debounceMs > kMaxDebounceMs) {
        return false;
    }
    if (channel.mode == GpioMode::Input && !pin->allowInput) {
        return false;
    }
    if (channel.mode == GpioMode::Output && !pin->allowOutput) {
        return false;
    }
    if (channel.pull == GpioPull::Up && !pin->allowPullup) {
        return false;
    }
    if (channel.pull == GpioPull::Down && !pin->allowPulldown) {
        return false;
    }
    return channel.mode == GpioMode::Disabled ||
           channel.mode == GpioMode::Input ||
           channel.mode == GpioMode::Output;
}

bool normalizeConfig(GpioData& data) {
    GpioData normalized{};
    applyDefaultConfig(normalized);

    bool ok = true;
    for (uint8_t i = 0; i < data.channelCount && i < kMaxChannels; i++) {
        const GpioChannelConfig& input = data.channels[i];
        const GpioPinDefinition* pin = findAllowedPinById(input.id);
        if (!pin || pin->pin != input.pin) {
            ok = false;
            continue;
        }

        for (uint8_t j = 0; j < normalized.channelCount; j++) {
            if (normalized.channels[j].pin != pin->pin) {
                continue;
            }

            normalized.channels[j] = input;
            strlcpy(normalized.channels[j].id, pin->id, sizeof(normalized.channels[j].id));
            normalized.channels[j].pin = pin->pin;
            if (normalized.channels[j].name[0] == '\0') {
                strlcpy(normalized.channels[j].name, pin->name, sizeof(normalized.channels[j].name));
            }
            if (normalized.channels[j].debounceMs > kMaxDebounceMs) {
                normalized.channels[j].debounceMs = kMaxDebounceMs;
            }
            if (!validateChannelConfig(normalized.channels[j])) {
                fillDefaultChannel(normalized.channels[j], *pin);
                ok = false;
            }
            break;
        }
    }

    for (uint8_t i = 0; i < normalized.channelCount; i++) {
        if (hasDuplicatePinOrId(normalized, i)) {
            ok = false;
            applyDefaultConfig(normalized);
            break;
        }
    }

    data = normalized;
    return ok;
}

}  // namespace GPIO
