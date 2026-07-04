#pragma once

#include "MatrixManagerTypes.h"

namespace MATRIX_MANAGER {

/**
 * @brief Manages Z-index layers for the Matrix display.
 * 
 * Each layer holds a LayerContent. Only the highest-priority active layer
 * is rendered at any time. Priority comes from the Layer enum value itself:
 * MENU is highest, BACKGROUND is lowest.
 *
 * MatrixManagerService owns this object and provides synchronization.
 */
class MatrixLayerManager {
public:
    MatrixLayerManager();

    /// Set content on a layer (marks it active).
    void setLayer(Layer layer, const LayerContent& content);

    /// Clear a layer (marks it inactive).
    void clearLayer(Layer layer);

    /// Get the highest-priority active layer. Returns false if all inactive.
    bool getTopLayer(LayerContent& out, Layer& outLayer) const;
    bool getTopLayer(LayerContent& out, Layer& outLayer, uint32_t& outHash) const;

    /// Check if a specific layer is active.
    bool isLayerActive(Layer layer) const;

    /// Get a copy of a specific layer's content, return true if active.
    bool getLayerContent(Layer layer, LayerContent& out) const;
    bool getLayerContent(Layer layer, LayerContent& out, uint32_t& outHash) const;

private:
    LayerContent _layers[LAYER_COUNT];
    uint32_t _hashes[LAYER_COUNT] = {0};
};

} // namespace MATRIX_MANAGER
