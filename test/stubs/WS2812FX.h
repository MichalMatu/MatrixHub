#pragma once

#include <cstdint>
#include <vector>

// Match the public vendor macros so native builds catch preprocessor name
// collisions that would otherwise surface only in the ESP32 toolchain.
#ifndef BRIGHTNESS_MIN
#define BRIGHTNESS_MIN (uint8_t)0
#endif
#ifndef BRIGHTNESS_MAX
#define BRIGHTNESS_MAX (uint8_t)255
#endif

#define FX_MODE_STATIC 0
#define NEO_RGB 0x10
#define NEO_GRB 0x20
#define NEO_KHZ800 0x01

class WS2812FX {
public:
    inline static WS2812FX* lastInstance = nullptr;

    WS2812FX(uint16_t pixelCount,
             uint8_t pin,
             uint8_t type,
             uint8_t maxSegments = 10,
             uint8_t maxActiveSegments = 10)
        : pixels(pixelCount, 0), shownPixels(pixelCount, 0), pin(pin), type(type),
          maxSegments(maxSegments), maxActiveSegments(maxActiveSegments) {
        lastInstance = this;
    }

    void init() { initialized = true; }
    bool service() {
        serviceCalls++;
        if (serviceRenderedFrame) {
            show();
        }
        return serviceRenderedFrame;
    }
    void show() {
        shownPixels = pixels;
        showCalls++;
    }
    void setPixelColor(uint16_t index, uint32_t color) {
        if (index < pixels.size()) {
            pixels[index] = scaleColor(color, brightness);
            setPixelColorCalls++;
        }
    }
    void setBrightness(uint8_t value) {
        if (value != brightness) {
            for (uint32_t& color : pixels) {
                color = rescaleColor(color, brightness, value);
            }
        }
        brightness = value;
        setBrightnessCalls++;
    }
    void setMode(uint8_t value) { mode = value; }
    void setSpeed(uint16_t value) { speed = value; }
    void setColors(uint8_t segment, uint32_t* values) {
        (void)segment;
        if (values) {
            colors[0] = values[0];
            colors[1] = values[1];
            colors[2] = values[2];
        }
    }
    void setSegment(uint8_t segment,
                    uint16_t start,
                    uint16_t stop,
                    uint8_t modeValue,
                    uint32_t color,
                    uint16_t speedValue,
                    bool reverse) {
        (void)segment;
        (void)start;
        (void)stop;
        (void)modeValue;
        (void)color;
        (void)speedValue;
        (void)reverse;
    }
    void setExtDataSrc(uint8_t segment, uint8_t* source, uint8_t size) {
        (void)segment;
        (void)source;
        (void)size;
    }
    void start() {
        running = true;
        startCalls++;
    }
    void trigger() { triggerCalls++; }
    void stop() {
        running = false;
        stopCalls++;
        pixels.assign(pixels.size(), 0);
        show();
    }
    void pause() {
        running = false;
        pauseCalls++;
    }
    bool isRunning() const { return running; }

    static uint8_t scaleChannel(uint8_t value, uint8_t brightness) {
        if (brightness == 255) {
            return value;
        }
        return static_cast<uint8_t>((static_cast<uint16_t>(value) *
                                     static_cast<uint16_t>(brightness + 1U)) >> 8U);
    }

    static uint32_t scaleColor(uint32_t color, uint8_t brightness) {
        return (static_cast<uint32_t>(scaleChannel((color >> 16U) & 0xFFU, brightness)) << 16U) |
               (static_cast<uint32_t>(scaleChannel((color >> 8U) & 0xFFU, brightness)) << 8U) |
               static_cast<uint32_t>(scaleChannel(color & 0xFFU, brightness));
    }

    static uint8_t rescaleTransportChannel(uint8_t value,
                                           uint8_t oldBrightness,
                                           uint8_t newBrightness) {
        uint16_t scale = 0;
        if (oldBrightness == 0) {
            scale = 0;
        } else if (newBrightness == 255) {
            scale = static_cast<uint16_t>(65535U / oldBrightness);
        } else {
            const uint16_t encodedNew = static_cast<uint16_t>(newBrightness) + 1U;
            scale = static_cast<uint16_t>(((encodedNew << 8U) - 1U) / oldBrightness);
        }
        return static_cast<uint8_t>((static_cast<uint16_t>(value) * scale) >> 8U);
    }

    static uint32_t rescaleColor(uint32_t color,
                                 uint8_t oldBrightness,
                                 uint8_t newBrightness) {
        return (static_cast<uint32_t>(rescaleTransportChannel(
                    (color >> 16U) & 0xFFU, oldBrightness, newBrightness)) << 16U) |
               (static_cast<uint32_t>(rescaleTransportChannel(
                    (color >> 8U) & 0xFFU, oldBrightness, newBrightness)) << 8U) |
               static_cast<uint32_t>(rescaleTransportChannel(
                    color & 0xFFU, oldBrightness, newBrightness));
    }

    std::vector<uint32_t> pixels;
    std::vector<uint32_t> shownPixels;
    uint8_t pin = 0;
    uint8_t type = 0;
    uint8_t maxSegments = 0;
    uint8_t maxActiveSegments = 0;
    uint8_t brightness = 255;
    uint8_t mode = 0;
    uint16_t speed = 0;
    uint32_t colors[3] = {0, 0, 0};
    uint32_t serviceCalls = 0;
    uint32_t showCalls = 0;
    uint32_t setPixelColorCalls = 0;
    uint32_t setBrightnessCalls = 0;
    uint32_t startCalls = 0;
    uint32_t triggerCalls = 0;
    uint32_t stopCalls = 0;
    uint32_t pauseCalls = 0;
    bool initialized = false;
    bool running = false;
    bool serviceRenderedFrame = true;
};
