#include "MatrixRenderer.h"
#include <esp_log.h>
#include "../assets/MatrixBitmaps.h"
#include "../../../src/matrix/MatrixEffectModes.h"
#include <WS2812FX.h>
#include <cstring>

static const char* TAG = "MatrixRenderer";

MatrixRenderer::MatrixRenderer()
    : _matrix(nullptr), _pin(0), _brightness(UI::MATRIX::BRIGHTNESS_DEFAULT),
      _scrolling(false), _effectRunning(false), _x(0), _minX(0), _lastScrollUpdate(0),
      _color(0xFFFFFF) {
}

void MatrixRenderer::blackout() {
    if (!_matrix) return;

    if (_effectRunning) {
        _matrix->stop();
        _effectRunning = false;
    }
    if (_nativeEffectRunning) {
        _nativeEngine.reset();
        _nativeEffectRunning = false;
    }
    if (_dataVisualizationRunning) {
        _dataVisualizationEngine.reset();
        _dataVisualizationRunning = false;
    }

    _scrolling = false;
    _activeIcon = IconType::NONE;
    _hasActiveIconBitmap = false;
    _matrix->fillScreen(0);
    _matrix->show();
}

void MatrixRenderer::begin(uint8_t pin) {
    _pin = pin;
    ESP_LOGI(TAG, "Initializing Matrix Renderer on pin %d", _pin);

    _matrix = new LedMatrix();
    _matrix->begin(_pin);
    _matrix->setBrightness(_brightness);
    
    // Configure WS2812FX segment for effects
    _matrix->setSegment(0, 0, UI::MATRIX::LED_COUNT - 1, FX_MODE_STATIC, 0x000000, UI::MATRIX::DEFAULT_EFFECT_SPEED, false);
    
    _matrix->stop();
    _matrix->fillScreen(0);
    _matrix->show();
    
    ESP_LOGI(TAG, "Matrix Initialized (minimal driver)");
}

void MatrixRenderer::loop() {
    if (!_matrix) return;
    if (isDisplayMuted()) return;
    
    // SCROLL MODE — simple single-draw with loop reset
    if (_scrolling) {
        uint32_t now = millis();
        if (now - _lastScrollUpdate < _scrollSpeed) {
            return;
        }
        _lastScrollUpdate = now;

        // Clear entire screen before redraw. On 8x8 (64 LEDs) this is
        // negligible vs show() overhead, and eliminates all edge artifacts.
        _matrix->fillScreen(0);
        _matrix->drawString(_x, 0, _text, _color, 0x000000, false);

        if (--_x < _minX) {
            // Text fully scrolled off — restart from right edge
            _x = _matrix->width();
        }
        
        _matrix->show();
    }
    // EFFECT MODE
    else if (_effectRunning) {
        _matrix->service();
    } else if (_nativeEffectRunning) {
        if (_nativeEngine.render(millis(), _nativeFrame, MATRIX_FX::kMatrixFxPixelCount)) {
            _matrix->drawBitmap(_nativeFrame);
            _matrix->show();
        }
    } else if (_dataVisualizationRunning) {
        if (_dataVisualizationEngine.render(millis(), _dataVisualizationFrame, MATRIX::kMatrixDataVizPixelCount)) {
            _matrix->drawBitmap(_dataVisualizationFrame);
            _matrix->show();
        }
    }
}

void MatrixRenderer::showText(const char* text, uint32_t color) {
    if (!_matrix || !text) return;
    if (isDisplayMuted()) {
        blackout();
        return;
    }
    
    // Stop FX if running
    if (_effectRunning) {
        _matrix->stop();
        _effectRunning = false;
    }
    if (_nativeEffectRunning) {
        _nativeEngine.reset();
        _nativeEffectRunning = false;
    }
    if (_dataVisualizationRunning) {
        _dataVisualizationEngine.reset();
        _dataVisualizationRunning = false;
    }
    _activeIcon = IconType::NONE;
    _hasActiveIconBitmap = false;
    
    // Minimal padding for visual spacing between loops
    char paddedText[kMatrixRenderedTextCapacity];
    snprintf(paddedText, sizeof(paddedText), "  %s  ", text);

    const bool sameText = _scrolling && strncmp(_text, paddedText, sizeof(_text)) == 0;
    // Color-only updates used to be skipped for the same text, leaving the old
    // color on screen until the message content changed.
    const bool colorChanged = (_color != color);

    // Prevent redundant repaint if the visible content is already identical.
    if (sameText && !colorChanged) {
        return;
    }

    const bool restartingScroll = !sameText;
    if (restartingScroll) {
        strlcpy(_text, paddedText, sizeof(_text));
        // Prepare the first visible frame so the next renderer loop can push it
        // immediately after the command phase, without writing to LEDs here.
        _x = _matrix->width() > 0 ? _matrix->width() - 1 : 0;
        _minX = -1 * LedMatrix::getStringWidth(_text);
    }

    _color = color;
    _scrolling = true;
    const uint32_t now = millis();

    // Force the very next loop() call to repaint immediately. On device this
    // still happens in the same MatrixService tick, but keeps all LED writes
    // inside the renderer loop instead of mixing command and render phases.
    _lastScrollUpdate = now - _scrollSpeed;
}


void MatrixRenderer::showSolid(uint32_t color) {
    if (!_matrix) return;
    if (isDisplayMuted()) {
        blackout();
        return;
    }
    
    if (_effectRunning) { _matrix->stop(); _effectRunning = false; }
    if (_nativeEffectRunning) { _nativeEngine.reset(); _nativeEffectRunning = false; }
    if (_dataVisualizationRunning) { _dataVisualizationEngine.reset(); _dataVisualizationRunning = false; }
    _scrolling = false;
    _activeIcon = IconType::NONE;
    _hasActiveIconBitmap = false;
    
    _matrix->fillScreen(color);
    _matrix->show();
}

void MatrixRenderer::clear() {
    if (!_matrix) return;
    
    blackout();
}

void MatrixRenderer::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    if (_matrix) {
        _matrix->setBrightness(brightness);

        if (isDisplayMuted()) {
            blackout();
        }
    }
}

void MatrixRenderer::setRotation(uint8_t rotation) {
    if (_matrix) {
        _matrix->setRotation(rotation);
        if (_scrolling) {
            _matrix->fillScreen(0);  // Clear stale pixels; scroll continues from current _x
        }
        // Re-render static icon after rotation change
        if (!_scrolling && !_effectRunning && !_nativeEffectRunning && !_dataVisualizationRunning && _activeIcon != IconType::NONE) {
            _matrix->fillScreen(0);
            IconDrawer::draw(_matrix, _activeIcon, _hasActiveIconBitmap ? _activeIconBitmap : nullptr);
            _matrix->show();
        }
    }
}

void MatrixRenderer::setScrollSpeed(uint16_t ms) {
    if (ms > 0) {
        _scrollSpeed = ms;
    }
}

bool MatrixRenderer::isActive() const {
    return _scrolling || _effectRunning || _nativeEffectRunning || _dataVisualizationRunning;
}

void MatrixRenderer::showIcon(IconType icon, const uint32_t* customBitmap) {
    if (!_matrix) return;
    if (isDisplayMuted()) {
        blackout();
        return;
    }
    
    if (_effectRunning) { _matrix->stop(); _effectRunning = false; }
    if (_nativeEffectRunning) { _nativeEngine.reset(); _nativeEffectRunning = false; }
    if (_dataVisualizationRunning) { _dataVisualizationEngine.reset(); _dataVisualizationRunning = false; }
    _scrolling = false;
    _activeIcon = icon;
    
    if (customBitmap) {
        memcpy(_activeIconBitmap, customBitmap, sizeof(uint32_t) * 64);
        _hasActiveIconBitmap = true;
    } else {
        _hasActiveIconBitmap = false;
    }
    
    IconDrawer::draw(_matrix, icon, customBitmap);
    _matrix->show();
}

void MatrixRenderer::showEffect(uint8_t mode, uint32_t speed, uint32_t color, uint32_t color2, uint32_t color3) {
    if (!_matrix) return;
    if (isDisplayMuted()) {
        blackout();
        return;
    }
    
    _activeIcon = IconType::NONE;
    _hasActiveIconBitmap = false;
    if (_nativeEffectRunning) {
        _nativeEngine.reset();
        _nativeEffectRunning = false;
    }
    if (_dataVisualizationRunning) {
        _dataVisualizationEngine.reset();
        _dataVisualizationRunning = false;
    }
    uint32_t colors[] = { color, color2, color3 };
    // Guard the renderer too, so invalid values from stale state or a future
    // caller still fall back to a known-good effect.
    const uint8_t normalizedMode = MATRIX::normalizeMatrixEffectMode(mode);
    
    _effectSpeedMs = speed < UI::MATRIX::MIN_EFFECT_SPEED
        ? UI::MATRIX::MIN_EFFECT_SPEED
        : (speed > UI::MATRIX::MAX_EFFECT_SPEED ? UI::MATRIX::MAX_EFFECT_SPEED : speed);

    _matrix->setMode(normalizedMode);
    _matrix->setSpeed(static_cast<uint16_t>(
        _effectSpeedMs > UI::MATRIX::EFFECT_DRIVER_SPEED_MAX
            ? UI::MATRIX::EFFECT_DRIVER_SPEED_MAX
            : _effectSpeedMs));
    _matrix->setColors(0, colors);
    
    if (!_effectRunning) {
        _matrix->start();
        _effectRunning = true;
    }
    _scrolling = false;
}

void MatrixRenderer::showNative3DEffect(uint8_t mode,
                                        uint32_t speed,
                                        uint32_t color,
                                        uint32_t color2,
                                        uint32_t color3,
                                        uint8_t reactivityProvider,
                                        uint8_t reactivityGain) {
    if (!_matrix) return;
    if (isDisplayMuted()) {
        blackout();
        return;
    }

    if (_effectRunning) {
        _matrix->stop();
        _effectRunning = false;
    }
    if (_dataVisualizationRunning) {
        _dataVisualizationEngine.reset();
        _dataVisualizationRunning = false;
    }
    _scrolling = false;
    _activeIcon = IconType::NONE;
    _hasActiveIconBitmap = false;

    MATRIX_FX::MatrixFxConfig config;
    config.mode = MATRIX_FX::normalizeNative3DMode(mode);
    config.speedMs = speed < UI::MATRIX::MIN_EFFECT_SPEED
        ? UI::MATRIX::MIN_EFFECT_SPEED
        : (speed > UI::MATRIX::MAX_EFFECT_SPEED ? UI::MATRIX::MAX_EFFECT_SPEED : speed);
    config.color1 = color;
    config.color2 = color2;
    config.color3 = color3;
    config.provider = static_cast<MATRIX_FX::ReactiveProvider>(
        MATRIX_FX::normalizeReactiveProvider(reactivityProvider));
    config.reactivityGain = MATRIX_FX::clampReactivityGain(reactivityGain);

    _nativeEngine.configure(config);
    _nativeEffectRunning = true;
    memset(_nativeFrame, 0, sizeof(_nativeFrame));
}

void MatrixRenderer::setEffectInput(const MATRIX_FX::MatrixFxInput& input) {
    _nativeEngine.setInput(input);
}

void MatrixRenderer::showDataVisualization(const MATRIX::MatrixDataVisualizationConfig& config) {
    if (!_matrix) return;
    if (isDisplayMuted()) {
        blackout();
        return;
    }

    if (_effectRunning) {
        _matrix->stop();
        _effectRunning = false;
    }
    if (_nativeEffectRunning) {
        _nativeEngine.reset();
        _nativeEffectRunning = false;
    }
    _scrolling = false;
    _activeIcon = IconType::NONE;
    _hasActiveIconBitmap = false;

    _dataVisualizationEngine.configure(config);
    _dataVisualizationRunning = true;
    memset(_dataVisualizationFrame, 0, sizeof(_dataVisualizationFrame));
}

void MatrixRenderer::setDataVisualizationInput(const MATRIX::MatrixDataVisualizationInput& input) {
    _dataVisualizationEngine.setInput(input);
}
