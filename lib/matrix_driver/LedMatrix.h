#pragma once

#include <Arduino.h>
#include <WS2812FX.h>

/**
 * @brief Minimal 8x8 LED Matrix wrapper using WS2812FX
 * Replaces heavy Adafruit_NeoMatrix + Adafruit_GFX stack
 */
class LedMatrix {
public:
    static constexpr uint8_t WIDTH = 8;
    static constexpr uint8_t HEIGHT = 8;
    static constexpr uint16_t NUM_LEDS = WIDTH * HEIGHT;
    
    LedMatrix()
        : _strip(nullptr),
          _brightness(50),
          _rotation(0),
          _outputMuted(false),
          _restorePending(false),
          _pausedEffectFramePending(false) {}
    
    /**
     * @brief Initialize the LED matrix
     * @param pin GPIO pin connected to LED data line
     */
    void begin(uint8_t pin);
    
    /**
     * @brief Must be called in loop() to service effects
     */
    void service();
    
    // ===== Drawing Primitives =====
    
    /**
     * @brief Set a single pixel color (respects rotation)
     */
    void setPixel(int16_t x, int16_t y, uint32_t color);
    
    /**
     * @brief Fill entire matrix with color
     */
    void fillScreen(uint32_t color);
    
    /**
     * @brief Draw an 8x8 bitmap (64 uint32_t colors, row-major)
     */
    void drawBitmap(const uint32_t* bitmap);
    
    /**
     * @brief Draw a single character at position
     * @param x X position (can be negative for scrolling)
     * @param y Y position (0 = top)
     * @param c Character to draw
     * @param color RGB888 color
     */
    void drawChar(int16_t x, int16_t y, char c, uint32_t color, uint32_t bg_color = 0, bool draw_bg = false);
    
    /**
     * @brief Draw a string at position
     */
    void drawString(int16_t x, int16_t y, const char* str, uint32_t color, uint32_t bg_color = 0, bool draw_bg = false);
    
    /**
     * @brief Get width of string in pixels
     */
    static int16_t getStringWidth(const char* str);
    
    /**
     * @brief Push changes to LEDs
     */
    void show();

    /** Commit the preserved static frame after a reversible output mute. */
    void restoreOutputIfPending();
    
    // ===== Configuration =====
    
    void setBrightness(uint8_t brightness);
    void setRotation(uint8_t rotation); // 0, 1, 2, 3 (0=normal, 1=90°CW, etc)
    uint8_t width() const { return WIDTH; }
    uint8_t height() const { return HEIGHT; }
    
    // ===== WS2812FX Effects =====
    
    void setMode(uint8_t mode);
    void setSpeed(uint16_t speed);
    void setColors(uint8_t seg, const uint32_t* colors);
    void setSegment(uint8_t seg, uint16_t start, uint16_t stop, uint8_t mode, uint32_t color, uint16_t speed, bool reverse);
    void setExtDataSrc(uint8_t seg, uint8_t* src, uint8_t size);
    void start();
    void pauseEffect();
    bool isRunning() const;

private:
    WS2812FX* _strip;
    uint8_t _brightness;
    uint8_t _rotation;
    bool _outputMuted;
    bool _restorePending;
    // A paused legacy effect leaves its last complete transport frame in the
    // strip buffer. Preserve it until the incoming owner commits a frame so a
    // concurrent brightness update cannot expose the logical black buffer.
    bool _pausedEffectFramePending;
    // Adafruit_NeoPixel stores brightness-scaled bytes, so changing brightness
    // cannot restore an exact static frame from the transport buffer. Keep the
    // 8x8 logical RGB frame here and treat the strip buffer as write-only.
    uint32_t _framebuffer[NUM_LEDS] = {};
    
    /**
     * @brief Convert x,y to linear LED index with rotation
     */
    uint16_t xy(int16_t x, int16_t y) const;
    void writeBlackOutputFrame();
    void writeLogicalOutputFrame();
};
