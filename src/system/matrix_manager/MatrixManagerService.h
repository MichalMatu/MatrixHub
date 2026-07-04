#pragma once

#include "MatrixManagerTypes.h"
#include "MatrixLayerManager.h"
#include "MatrixNotificationQueue.h"

class MatrixService;

namespace MATRIX_MANAGER {

/**
 * @brief The "Director" — orchestrates what is displayed on the Matrix.
 * 
 * Sits between consumers (Alarms, Menu, API) and the dumb MatrixService renderer.
 * Manages:
 * - layer priority through MatrixLayerManager
 * - queued transient notifications through MatrixNotificationQueue
 * - change detection so the renderer is only poked when visible content changes
 *
 * Layer timeouts are measured only while a layer is actually the top visible
 * layer. That avoids expiring content "in the background" while a higher-
 * priority layer is temporarily covering it.
 * 
 * Call update() from the MatrixTask loop before MatrixService::loop().
 */
class MatrixManagerService {
public:
    explicit MatrixManagerService(MatrixService* matrixService);
    ~MatrixManagerService();

    // ─── Layer API (for consumers) ───────────────────────────────
    
    /// Set content on a layer (marks it active).
    void setLayer(Layer layer, const LayerContent& content);
    
    /// Clear a layer (marks it inactive).
    void clearLayer(Layer layer);
    
    /// Check if a specific layer is active.
    bool isLayerActive(Layer layer) const;

    // ─── Notification Queue API ──────────────────────────────────
    
    /// Push a text notification to the FIFO queue.
    void queueNotification(const char* text, uint32_t color, uint32_t displayMs = 3000);
    
    /// Get number of pending notifications.
    uint8_t pendingNotifications() const;

    // ─── Task Loop ───────────────────────────────────────────────
    
    /// Called from MatrixTask loop. Evaluates layers + queue, sends
    /// commands to MatrixService. Call BEFORE MatrixService::loop().
    void update();

    // ─── Direct access (for advanced consumers like MatrixMenuService) ──
    MatrixService* getMatrixService() const { return _matrixService; }

    /// Force a re-render of the top layer on the next update
    void invalidateCache();

private:
    MatrixService* _matrixService;

    MatrixLayerManager _layerManager;
    MatrixNotificationQueue _queue;
    SemaphoreHandle_t _mutex = nullptr;
    
    // Notification timer state (for queue auto-advance)
    uint32_t _notifDurationMs = 0;
    bool _notifActive = false;
    
    // Generic Layer timeouts (tracks how long a layer has been the TOP VISIBLE layer)
    uint32_t _layerVisibleTimeMs[LAYER_COUNT] = {0};
    
    // Change detection: track what we last sent to renderer
    Layer _lastRenderedLayer = Layer::BACKGROUND;
    CommandType _lastRenderedType = CommandType::NONE;
    uint32_t _lastContentHash = 0;
    bool _wasRendering = false;
    LayerContent _lastRenderedContent;
    
    uint32_t _lastUpdateMs = 0;
    
    // Internal helpers
    void checkLayerTimeouts();
    void advanceNotificationQueue();
    void applyLayerToRenderer(const LayerContent& content);
};

} // namespace MATRIX_MANAGER
