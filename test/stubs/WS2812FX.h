#pragma once

#include <cstdint>
#include <vector>

#define FX_MODE_STATIC 0
#define NEO_RGB 0x00
#define NEO_KHZ800 0x00

class WS2812FX {
public:
    inline static WS2812FX* lastInstance = nullptr;

    WS2812FX(uint16_t pixelCount, uint8_t pin, uint8_t type)
        : pixels(pixelCount, 0), shownPixels(pixelCount, 0), pin(pin), type(type) {
        lastInstance = this;
    }

    void init() { initialized = true; }
    void service() { serviceCalls++; }
    void show() {
        shownPixels = pixels;
        showCalls++;
    }
    void setPixelColor(uint16_t index, uint32_t color) {
        if (index < pixels.size()) {
            pixels[index] = color;
        }
    }
    void setBrightness(uint8_t value) { brightness = value; }
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
    void start() { running = true; }
    void stop() { running = false; }
    bool isRunning() const { return running; }

    std::vector<uint32_t> pixels;
    std::vector<uint32_t> shownPixels;
    uint8_t pin = 0;
    uint8_t type = 0;
    uint8_t brightness = 0;
    uint8_t mode = 0;
    uint16_t speed = 0;
    uint32_t colors[3] = {0, 0, 0};
    uint32_t serviceCalls = 0;
    uint32_t showCalls = 0;
    bool initialized = false;
    bool running = false;
};
