#pragma once

#include <Arduino.h>

#include <cstdint>
#include <cstdio>
#include <cstring>

class LedMatrix {
public:
    static constexpr uint8_t WIDTH = 8;
    static constexpr uint8_t HEIGHT = 8;

    inline static LedMatrix* lastInstance = nullptr;

    LedMatrix() {
        lastInstance = this;
    }

    void begin(uint8_t pin) { begunPin = pin; }
    void service() { serviceCalls++; }
    void setPixel(int16_t x, int16_t y, uint32_t color) {
        lastPixelX = x;
        lastPixelY = y;
        lastPixelColor = color;
        setPixelCalls++;
    }
    void fillScreen(uint32_t color) {
        lastFillColor = color;
        fillCalls++;
    }
    void drawBitmap(const uint32_t* bitmap) {
        lastBitmapWasNull = (bitmap == nullptr);
        drawBitmapCalls++;
    }
    void drawChar(int16_t x, int16_t y, char c, uint32_t color, uint32_t bg_color = 0, bool draw_bg = false) {
        (void)x;
        (void)y;
        (void)c;
        (void)color;
        (void)bg_color;
        (void)draw_bg;
    }
    void drawString(int16_t x, int16_t y, const char* str, uint32_t color, uint32_t bg_color = 0, bool draw_bg = false) {
        (void)bg_color;
        (void)draw_bg;
        lastDrawX = x;
        lastDrawY = y;
        lastDrawColor = color;
        std::snprintf(lastText, sizeof(lastText), "%s", str ? str : "");
        drawStringCalls++;
    }

    static int16_t getStringWidth(const char* str) {
        return str ? static_cast<int16_t>(std::strlen(str) * 6) : 0;
    }

    void show() { showCalls++; }
    void setBrightness(uint8_t brightness) { lastBrightness = brightness; }
    void setRotation(uint8_t rotation) { lastRotation = rotation; }
    uint8_t width() const { return WIDTH; }
    uint8_t height() const { return HEIGHT; }

    void setMode(uint8_t mode) { lastMode = mode; }
    void setSpeed(uint16_t speed) { lastSpeed = speed; }
    void setColors(uint8_t seg, const uint32_t* colors) {
        lastColorSegment = seg;
        lastColor1 = colors ? colors[0] : 0;
        lastColor2 = colors ? colors[1] : 0;
        lastColor3 = colors ? colors[2] : 0;
    }
    void setSegment(uint8_t seg, uint16_t start, uint16_t stop, uint8_t mode, uint32_t color, uint16_t speed, bool reverse) {
        lastSegment = seg;
        lastSegmentStart = start;
        lastSegmentStop = stop;
        lastSegmentMode = mode;
        lastSegmentColor = color;
        lastSegmentSpeed = speed;
        lastSegmentReverse = reverse;
    }
    void setExtDataSrc(uint8_t seg, uint8_t* src, uint8_t size) {
        (void)seg;
        (void)src;
        (void)size;
    }
    void start() {
        running = true;
        startCalls++;
    }
    void stop() {
        running = false;
        stopCalls++;
    }
    bool isRunning() const { return running; }

    void resetCounters() {
        showCalls = 0;
        fillCalls = 0;
        drawStringCalls = 0;
        drawBitmapCalls = 0;
        serviceCalls = 0;
        startCalls = 0;
        stopCalls = 0;
        setPixelCalls = 0;
    }

    uint8_t begunPin = 0;
    uint8_t lastBrightness = 0;
    uint8_t lastRotation = 0;
    uint8_t lastMode = 0;
    uint16_t lastSpeed = 0;
    uint8_t lastColorSegment = 0;
    uint32_t lastColor1 = 0;
    uint32_t lastColor2 = 0;
    uint32_t lastColor3 = 0;
    uint8_t lastSegment = 0;
    uint16_t lastSegmentStart = 0;
    uint16_t lastSegmentStop = 0;
    uint8_t lastSegmentMode = 0;
    uint32_t lastSegmentColor = 0;
    uint16_t lastSegmentSpeed = 0;
    bool lastSegmentReverse = false;
    uint32_t lastFillColor = 0;
    int16_t lastDrawX = 0;
    int16_t lastDrawY = 0;
    uint32_t lastDrawColor = 0;
    char lastText[96] = {0};
    bool lastBitmapWasNull = false;
    int16_t lastPixelX = 0;
    int16_t lastPixelY = 0;
    uint32_t lastPixelColor = 0;
    uint32_t showCalls = 0;
    uint32_t fillCalls = 0;
    uint32_t drawStringCalls = 0;
    uint32_t drawBitmapCalls = 0;
    uint32_t serviceCalls = 0;
    uint32_t startCalls = 0;
    uint32_t stopCalls = 0;
    uint32_t setPixelCalls = 0;
    bool running = false;
};
