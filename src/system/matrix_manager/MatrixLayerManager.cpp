#include "MatrixLayerManager.h"

#include <cstddef>
#include <cstring>

namespace MATRIX_MANAGER {

namespace {

uint32_t computeLayerHash(const LayerContent& content) {
    uint32_t hash = 2166136261u;

    auto mix = [&hash](uint32_t val) {
        hash ^= val;
        hash *= 16777619u;
    };
    auto mixFloat = [&mix](float val) {
        uint32_t bits = 0;
        std::memcpy(&bits, &val, sizeof(bits));
        mix(bits);
    };

    mix(static_cast<uint32_t>(content.type));
    mix(content.color);
    mix(static_cast<uint32_t>(content.icon));
    mix(content.effectEngine);
    mix(content.effectMode);
    mix(content.effectSpeed);
    mix(content.effectColor);
    mix(content.effectColor2);
    mix(content.effectColor3);
    mix(content.effectReactivityProvider);
    mix(content.effectReactivityGain);
    mix(content.dataVisualizationConfig.enabled ? 1u : 0u);
    mix(content.dataVisualizationConfig.source);
    mix(content.dataVisualizationConfig.metric);
    mix(content.dataVisualizationConfig.mode);
    mixFloat(content.dataVisualizationConfig.minValue);
    mixFloat(content.dataVisualizationConfig.maxValue);
    mix(content.dataVisualizationConfig.colorMin);
    mix(content.dataVisualizationConfig.colorMid);
    mix(content.dataVisualizationConfig.colorMax);
    mix(content.dataVisualizationConfig.brightnessMin);
    mix(content.dataVisualizationConfig.brightnessMax);
    mix(content.dataVisualizationConfig.smoothing);
    mix(content.dataVisualizationConfig.staleBehavior);
    for (size_t i = 0; i < MATRIX::kMatrixDataVizDeviceIdCapacity &&
                       content.dataVisualizationConfig.deviceId[i] != '\0'; ++i) {
        mix(static_cast<uint32_t>(content.dataVisualizationConfig.deviceId[i]));
    }

    for (size_t i = 0; content.text[i] != '\0'; ++i) {
        mix(static_cast<uint32_t>(content.text[i]));
    }

    return hash;
}

} // namespace

MatrixLayerManager::MatrixLayerManager() {
    for (uint8_t i = 0; i < LAYER_COUNT; i++) {
        _layers[i].clear();
        _hashes[i] = 0;
    }
}

void MatrixLayerManager::setLayer(Layer layer, const LayerContent& content) {
    uint8_t idx = static_cast<uint8_t>(layer);
    if (idx >= LAYER_COUNT) return;

    _layers[idx] = content;
    _layers[idx].active = true;
    _hashes[idx] = computeLayerHash(_layers[idx]);
}

void MatrixLayerManager::clearLayer(Layer layer) {
    uint8_t idx = static_cast<uint8_t>(layer);
    if (idx >= LAYER_COUNT) return;

    _layers[idx].clear();
    _hashes[idx] = 0;
}

bool MatrixLayerManager::getTopLayer(LayerContent& out, Layer& outLayer) const {
    uint32_t ignoredHash = 0;
    return getTopLayer(out, outLayer, ignoredHash);
}

bool MatrixLayerManager::getTopLayer(LayerContent& out, Layer& outLayer, uint32_t& outHash) const {
    // Iterate from highest priority (MENU=5) down to BACKGROUND=0.
    for (int8_t i = LAYER_COUNT - 1; i >= 0; i--) {
        if (_layers[i].active) {
            out = _layers[i];
            outLayer = static_cast<Layer>(i);
            outHash = _hashes[i];
            return true;
        }
    }

    outHash = 0;
    return false;
}

bool MatrixLayerManager::isLayerActive(Layer layer) const {
    uint8_t idx = static_cast<uint8_t>(layer);
    if (idx >= LAYER_COUNT) return false;

    return _layers[idx].active;
}

bool MatrixLayerManager::getLayerContent(Layer layer, LayerContent& out) const {
    uint32_t ignoredHash = 0;
    return getLayerContent(layer, out, ignoredHash);
}

bool MatrixLayerManager::getLayerContent(Layer layer, LayerContent& out, uint32_t& outHash) const {
    uint8_t idx = static_cast<uint8_t>(layer);
    if (idx >= LAYER_COUNT) return false;

    if (_layers[idx].active) {
        out = _layers[idx];
        outHash = _hashes[idx];
        return true;
    }
    outHash = 0;
    return false;
}

} // namespace MATRIX_MANAGER
