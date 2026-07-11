#include "GpioConfigJson.h"

#include "../App.h"
#include "../../gpio/GpioConfigStore.h"
#include "../../gpio/GpioSafePins.h"

#include <cstring>

namespace CONFIG {
namespace JSON {

namespace {

bool readChannel(JsonObject& obj, GPIO::GpioChannelConfig& out) {
    const char* id = obj[Keys::kId] | static_cast<const char*>(nullptr);
    const auto* pinDef = GPIO::findAllowedPinById(id);
    if (!pinDef) {
        return false;
    }

    out = GPIO::GpioChannelConfig();
    strlcpy(out.id, pinDef->id, sizeof(out.id));
    strlcpy(out.name, pinDef->name, sizeof(out.name));
    out.pin = pinDef->pin;

    if (obj[Keys::kPin].is<int>() && obj[Keys::kPin].as<int>() != pinDef->pin) {
        return false;
    }

    if (const char* name = obj[Keys::kName] | static_cast<const char*>(nullptr)) {
        if (!GPIO::copySafeName(out.name, sizeof(out.name), name)) {
            return false;
        }
    }

    if (!obj[Keys::kMode].isNull()) {
        GPIO::GpioMode mode = out.mode;
        if (obj[Keys::kMode].is<const char*>()) {
            if (!GPIO::stringToMode(obj[Keys::kMode].as<const char*>(), mode)) {
                return false;
            }
        } else if (obj[Keys::kMode].is<int>()) {
            const int raw = obj[Keys::kMode].as<int>();
            if (raw < static_cast<int>(GPIO::GpioMode::Disabled) ||
                raw > static_cast<int>(GPIO::GpioMode::Output)) {
                return false;
            }
            mode = static_cast<GPIO::GpioMode>(raw);
        } else {
            return false;
        }
        out.mode = mode;
    }

    if (!obj[Keys::kPull].isNull()) {
        GPIO::GpioPull pull = out.pull;
        if (obj[Keys::kPull].is<const char*>()) {
            if (!GPIO::stringToPull(obj[Keys::kPull].as<const char*>(), pull)) {
                return false;
            }
        } else if (obj[Keys::kPull].is<int>()) {
            const int raw = obj[Keys::kPull].as<int>();
            if (raw < static_cast<int>(GPIO::GpioPull::None) ||
                raw > static_cast<int>(GPIO::GpioPull::Down)) {
                return false;
            }
            pull = static_cast<GPIO::GpioPull>(raw);
        } else {
            return false;
        }
        out.pull = pull;
    }

    if (obj[Keys::kInverted].is<bool>()) {
        out.inverted = obj[Keys::kInverted].as<bool>();
    }

    if (obj[Keys::kDebounceMs].is<int>()) {
        int raw = obj[Keys::kDebounceMs].as<int>();
        if (raw < GPIO::kMinDebounceMs) {
            raw = GPIO::kMinDebounceMs;
        }
        if (raw > GPIO::kMaxDebounceMs) {
            raw = GPIO::kMaxDebounceMs;
        }
        out.debounceMs = static_cast<uint16_t>(raw);
    }

    if (obj[Keys::kInitialOutput].is<bool>()) {
        out.initialOutput = obj[Keys::kInitialOutput].as<bool>();
    }

    return GPIO::validateChannelConfig(out);
}

void writeChannel(JsonObject& obj, const GPIO::GpioChannelConfig& channel) {
    obj[Keys::kId].set(String(channel.id));
    obj[Keys::kName].set(String(channel.name));
    obj[Keys::kPin] = channel.pin;
    obj[Keys::kMode].set(GPIO::modeToString(channel.mode));
    obj[Keys::kPull].set(GPIO::pullToString(channel.pull));
    obj[Keys::kInverted] = channel.inverted;
    obj[Keys::kDebounceMs] = channel.debounceMs;
    obj[Keys::kInitialOutput] = channel.initialOutput;
}

}  // namespace

bool deserializeGpio(JsonObject& obj, GPIO::GpioData& data) {
    GPIO::GpioData parsed{};
    GPIO::applyDefaultConfig(parsed);

    if (obj[Keys::kChannels].is<JsonArray>()) {
        for (JsonObject channelObj : obj[Keys::kChannels].as<JsonArray>()) {
            GPIO::GpioChannelConfig channel;
            if (!readChannel(channelObj, channel)) {
                return false;
            }

            bool matched = false;
            for (uint8_t i = 0; i < parsed.channelCount; i++) {
                if (strncmp(parsed.channels[i].id, channel.id, GPIO::kMaxIdLen) == 0) {
                    parsed.channels[i] = channel;
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return false;
            }
        }
    }

    data = parsed;
    return GPIO::normalizeConfig(data);
}

bool loadGpio(JsonObject& obj) {
    GPIO::GpioData next = GPIO::CONFIG_STORE::copy();
    if (!deserializeGpio(obj, next)) {
        return false;
    }

    return GPIO::CONFIG_STORE::update([&](GPIO::GpioData& cfg) {
        cfg = next;
    });
}

void saveGpio(JsonObject& obj) {
    const GPIO::GpioData data = GPIO::CONFIG_STORE::copy();
    JsonArray channels = obj[Keys::kChannels].to<JsonArray>();
    for (uint8_t i = 0; i < data.channelCount && i < GPIO::kMaxChannels; i++) {
        JsonObject channel = channels.add<JsonObject>();
        writeChannel(channel, data.channels[i]);
    }
}

}  // namespace JSON
}  // namespace CONFIG
