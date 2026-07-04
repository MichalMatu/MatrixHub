#include "LedMatrix.h"
#include "Font5x7.h"
#include <esp_log.h>

static const char* TAG = "LedMatrix";

#ifndef MATRIX_NEOPIXEL_TYPE
// This Waveshare panel is wired for RGB byte order.
// Do not "normalize" this to NEO_GRB just because many WS2812 strips use GRB:
// that swap was a real regression here and made red render as green.
// Keep the override hook so different panels can opt in at build time instead
// of editing this driver and accidentally breaking the board default again.
#define MATRIX_NEOPIXEL_TYPE (NEO_RGB + NEO_KHZ800)
#endif

void LedMatrix::begin(uint8_t pin) {
    ESP_LOGI(TAG, "Initializing LedMatrix on pin %d", pin);
    
    // Create WS2812FX instance
    // The renderer passes logical RGB888 colors end-to-end, so the NeoPixel
    // byte order here must stay aligned with the physical panel wiring.
    _strip = new WS2812FX(NUM_LEDS, pin, MATRIX_NEOPIXEL_TYPE);
    
    _strip->init();
    _strip->setBrightness(_brightness == 0 ? 1 : _brightness);
    
    // Default segment: all LEDs, static black
    _strip->setSegment(0, 0, NUM_LEDS - 1, FX_MODE_STATIC, (uint32_t)0x000000, 1000, (bool)false);
    _strip->stop();
    
    fillScreen(0);
    show();
    
    ESP_LOGI(TAG, "LedMatrix initialized (%dx%d = %d LEDs)", WIDTH, HEIGHT, NUM_LEDS);
}

void LedMatrix::service() {
    if (_strip && !_outputMuted) {
        _strip->service();
    }
}

uint16_t LedMatrix::xy(int16_t x, int16_t y) const {
    // Apply rotation
    int16_t rx, ry;
    switch (_rotation) {
        case 1: // 90° CW
            rx = WIDTH - 1 - y;
            ry = x;
            break;
        case 2: // 180°
            rx = WIDTH - 1 - x;
            ry = HEIGHT - 1 - y;
            break;
        case 3: // 270° CW (90° CCW)
            rx = y;
            ry = HEIGHT - 1 - x;
            break;
        default: // 0 - no rotation
            rx = x;
            ry = y;
            break;
    }
    
    // Matrix layout: NEO_MATRIX_TOP + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE
    // This means: top-right origin, columns run vertically, progressive (not zigzag)
    // LED index = column * HEIGHT + row
    // Column 0 is rightmost, increases to left
    int16_t col = WIDTH - 1 - rx;  // Flip X for right-origin
    return col * HEIGHT + ry;
}

void LedMatrix::setPixel(int16_t x, int16_t y, uint32_t color) {
    if (!_strip) return;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    
    _strip->setPixelColor(xy(x, y), color);
}

void LedMatrix::fillScreen(uint32_t color) {
    if (!_strip) return;
    
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        _strip->setPixelColor(i, color);
    }
}

void LedMatrix::drawBitmap(const uint32_t* bitmap) {
    if (!_strip || !bitmap) return;
    
    for (uint8_t y = 0; y < HEIGHT; y++) {
        for (uint8_t x = 0; x < WIDTH; x++) {
            setPixel(x, y, bitmap[y * WIDTH + x]);
        }
    }
}

void LedMatrix::drawChar(int16_t x, int16_t y, char c, uint32_t color, uint32_t bg_color, bool draw_bg) {
    const uint8_t* glyph = Font5x7::getGlyph(c);
    if (!glyph) return;
    
    // Font is stored as columns (5 bytes per char), LSB = top row
    for (int8_t col = 0; col < Font5x7::CHAR_WIDTH; col++) {
        uint8_t colData = pgm_read_byte(&glyph[col]);
        int16_t px = x + col;
        
        for (int8_t row = 0; row < Font5x7::CHAR_HEIGHT; row++) {
            if (colData & (1 << row)) {
                setPixel(px, y + row, color);
            } else if (draw_bg) {
                setPixel(px, y + row, bg_color);
            }
        }
    }
    // Draw character spacing column (always background)
    if (draw_bg) {
        int16_t px = x + Font5x7::CHAR_WIDTH;
        for (int8_t row = 0; row < Font5x7::CHAR_HEIGHT; row++) {
            setPixel(px, y + row, bg_color);
        }
    }
}

void LedMatrix::drawString(int16_t x, int16_t y, const char* str, uint32_t color, uint32_t bg_color, bool draw_bg) {
    if (!str) return;
    
    int16_t cursorX = x;
    while (*str) {
        // Only draw chars that are at least partially on-screen
        if (cursorX + Font5x7::CHAR_WIDTH > 0 && cursorX < WIDTH) {
            drawChar(cursorX, y, *str, color, bg_color, draw_bg);
        }
        
        cursorX += Font5x7::GLYPH_WIDTH;
        str++;
    }
}

int16_t LedMatrix::getStringWidth(const char* str) {
    if (!str) return 0;
    int16_t len = strlen(str);
    if (len == 0) return 0;
    return len * Font5x7::GLYPH_WIDTH;
}

void LedMatrix::show() {
    if (_strip) {
        if (_outputMuted) {
            fillScreen(0);
        }
        _strip->show();
    }
}

void LedMatrix::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    _outputMuted = (brightness == 0);
    if (_strip) {
        // Adafruit_NeoPixel rolls 0 over to full brightness internally.
        // Keep the hardware path on a safe non-zero value and enforce "off"
        // by muting output separately.
        _strip->setBrightness(_outputMuted ? 1 : brightness);

        if (_outputMuted) {
            fillScreen(0);
            _strip->show();
        }
    }
}

void LedMatrix::setRotation(uint8_t rotation) {
    _rotation = rotation % 4;
}

// ===== WS2812FX Effect Delegation =====

void LedMatrix::setMode(uint8_t mode) {
    if (_strip) _strip->setMode(mode);
}

void LedMatrix::setSpeed(uint16_t speed) {
    if (_strip) _strip->setSpeed(speed);
}

void LedMatrix::setColors(uint8_t seg, const uint32_t* colors) {
    if (_strip) _strip->setColors(seg, (uint32_t*)colors);
}

void LedMatrix::setSegment(uint8_t seg, uint16_t start, uint16_t stop, uint8_t mode, uint32_t color, uint16_t speed, bool reverse) {
    if (_strip) _strip->setSegment(seg, start, stop, mode, color, speed, reverse);
}

void LedMatrix::setExtDataSrc(uint8_t seg, uint8_t* src, uint8_t size) {
    if (_strip) _strip->setExtDataSrc(seg, src, size);
}

void LedMatrix::start() {
    if (_strip) _strip->start();
}

void LedMatrix::stop() {
    if (_strip) _strip->stop();
}

bool LedMatrix::isRunning() const {
    return _strip ? _strip->isRunning() : false;
}
