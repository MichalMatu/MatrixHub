#pragma once

#include <Arduino.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define NEO_RGB ((0 << 6) | (0 << 4) | (1 << 2) | 2)
#define NEO_RBG ((0 << 6) | (0 << 4) | (2 << 2) | 1)
#define NEO_GRB ((1 << 6) | (1 << 4) | (0 << 2) | 2)
#define NEO_GBR ((2 << 6) | (2 << 4) | (0 << 2) | 1)
#define NEO_BRG ((1 << 6) | (1 << 4) | (2 << 2) | 0)
#define NEO_BGR ((2 << 6) | (2 << 4) | (1 << 2) | 0)
#define NEO_KHZ800 0x0000
#define NEO_KHZ400 0x0100

using neoPixelType = uint16_t;

// Host-only transport-buffer model used when a native suite includes the real
// WS2812FX sources. Ordinary Matrix driver tests still resolve their dedicated
// test/stubs/WS2812FX.h facade and do not depend on this class.
class Adafruit_NeoPixel {
public:
    inline static uint32_t outOfBoundsWrites = 0;
    inline static uint32_t showCalls = 0;

    Adafruit_NeoPixel(uint16_t count,
                      int16_t outputPin = 6,
                      neoPixelType type = NEO_GRB + NEO_KHZ800)
        : pin(outputPin) {
        updateType(type);
        updateLength(count);
    }

    Adafruit_NeoPixel() = default;

    ~Adafruit_NeoPixel() {
        std::free(pixels);
    }

    bool begin() {
        if (pin < 0) {
            return false;
        }
        begun = true;
        return true;
    }

    void show() {
        ++showCalls;
    }

    void setPin(int16_t value) {
        pin = value;
    }

    void setPixelColor(uint16_t index, uint8_t red, uint8_t green, uint8_t blue) {
        setPixelColor(index, red, green, blue, 0);
    }

    void setPixelColor(
        uint16_t index,
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t white) {
        if (index >= numLEDs || pixels == nullptr) {
            ++outOfBoundsWrites;
            return;
        }

        if (brightness != 0) {
            red = scaleChannel(red, brightness);
            green = scaleChannel(green, brightness);
            blue = scaleChannel(blue, brightness);
            white = scaleChannel(white, brightness);
        }

        uint8_t* pixel = &pixels[index * bytesPerPixel()];
        pixel[rOffset] = red;
        pixel[gOffset] = green;
        pixel[bOffset] = blue;
        if (wOffset != rOffset) {
            pixel[wOffset] = white;
        }
    }

    void setPixelColor(uint16_t index, uint32_t color) {
        setPixelColor(
            index,
            static_cast<uint8_t>(color >> 16U),
            static_cast<uint8_t>(color >> 8U),
            static_cast<uint8_t>(color),
            static_cast<uint8_t>(color >> 24U));
    }

    void fill(uint32_t color = 0, uint16_t first = 0, uint16_t count = 0) {
        if (first >= numLEDs) {
            return;
        }
        const uint16_t end = count == 0
            ? numLEDs
            : static_cast<uint16_t>(std::min<uint32_t>(numLEDs, first + count));
        for (uint16_t index = first; index < end; ++index) {
            setPixelColor(index, color);
        }
    }

    void setBrightness(uint8_t value) {
        const uint8_t encoded = static_cast<uint8_t>(value + 1U);
        if (encoded == brightness || pixels == nullptr) {
            brightness = encoded;
            return;
        }

        const uint8_t oldExternal = static_cast<uint8_t>(brightness - 1U);
        const uint16_t scale = oldExternal == 0
            ? 0
            : (value == 255
                ? static_cast<uint16_t>(65535U / oldExternal)
                : static_cast<uint16_t>(((static_cast<uint16_t>(encoded) << 8U) - 1U) /
                                        oldExternal));
        for (uint16_t index = 0; index < numBytes; ++index) {
            pixels[index] = static_cast<uint8_t>(
                (static_cast<uint16_t>(pixels[index]) * scale) >> 8U);
        }
        brightness = encoded;
    }

    void clear() {
        if (pixels != nullptr) {
            std::memset(pixels, 0, numBytes);
        }
    }

    void updateLength(uint16_t count) {
        std::free(pixels);
        pixels = nullptr;
        numLEDs = 0;
        numBytes = static_cast<uint16_t>(count * bytesPerPixel());
        if (numBytes == 0) {
            return;
        }
        pixels = static_cast<uint8_t*>(std::calloc(numBytes, 1));
        if (pixels != nullptr) {
            numLEDs = count;
        } else {
            numBytes = 0;
        }
    }

    void updateType(neoPixelType type) {
        const bool usedThreeBytes = wOffset == rOffset;
        wOffset = static_cast<uint8_t>((type >> 6U) & 0x03U);
        rOffset = static_cast<uint8_t>((type >> 4U) & 0x03U);
        gOffset = static_cast<uint8_t>((type >> 2U) & 0x03U);
        bOffset = static_cast<uint8_t>(type & 0x03U);
        if (pixels != nullptr && usedThreeBytes != (wOffset == rOffset)) {
            updateLength(numLEDs);
        }
    }

    uint8_t* getPixels() const {
        return pixels;
    }

    uint8_t getBrightness() const {
        return static_cast<uint8_t>(brightness - 1U);
    }

    int16_t getPin() const {
        return pin;
    }

    uint16_t numPixels() const {
        return numLEDs;
    }

    uint32_t getPixelColor(uint16_t index) const {
        if (index >= numLEDs || pixels == nullptr) {
            return 0;
        }
        const uint8_t* pixel = &pixels[index * bytesPerPixel()];
        const auto restoreChannel = [this](uint8_t value) -> uint32_t {
            return brightness == 0
                ? value
                : (static_cast<uint32_t>(value) << 8U) / brightness;
        };
        return (static_cast<uint32_t>(
                    wOffset == rOffset ? 0 : restoreChannel(pixel[wOffset])) << 24U) |
               (static_cast<uint32_t>(restoreChannel(pixel[rOffset])) << 16U) |
               (static_cast<uint32_t>(restoreChannel(pixel[gOffset])) << 8U) |
               static_cast<uint32_t>(restoreChannel(pixel[bOffset]));
    }

    static uint8_t sine8(uint8_t value) {
        constexpr double kPi = 3.14159265358979323846;
        return static_cast<uint8_t>(
            (std::sin(static_cast<double>(value) * kPi / 128.0) + 1.0) * 127.5 + 0.5);
    }

    static uint8_t gamma8(uint8_t value) {
        return static_cast<uint8_t>(
            std::pow(static_cast<double>(value) / 255.0, 2.6) * 255.0 + 0.5);
    }

    static uint32_t Color(uint8_t red, uint8_t green, uint8_t blue) {
        return (static_cast<uint32_t>(red) << 16U) |
               (static_cast<uint32_t>(green) << 8U) |
               blue;
    }

private:
    static uint8_t scaleChannel(uint8_t value, uint8_t encodedBrightness) {
        return static_cast<uint8_t>(
            (static_cast<uint16_t>(value) * encodedBrightness) >> 8U);
    }

    uint8_t bytesPerPixel() const {
        return wOffset == rOffset ? 3U : 4U;
    }

protected:
    bool begun = false;
    uint16_t numLEDs = 0;
    uint16_t numBytes = 0;
    int16_t pin = -1;
    uint8_t brightness = 0;
    uint8_t* pixels = nullptr;
    uint8_t rOffset = 1;
    uint8_t gOffset = 0;
    uint8_t bOffset = 2;
    uint8_t wOffset = 1;
};
