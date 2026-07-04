#include "MatrixManagerService.h"
#include <MatrixService.h>
#include "../logging/Logging.h"
#include "../utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "MatrixMgr"

namespace MATRIX_MANAGER {

MatrixManagerService::MatrixManagerService(MatrixService* matrixService)
    : _matrixService(matrixService) {
    configASSERT(matrixService != nullptr);
    _mutex = xSemaphoreCreateRecursiveMutex();
    if (!_mutex) {
        LOGE("Failed to create matrix manager mutex");
    }
}

MatrixManagerService::~MatrixManagerService() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

// ─── Layer API ───────────────────────────────────────────────────

void MatrixManagerService::setLayer(Layer layer, const LayerContent& content) {
    SYSTEM::RecursiveScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        LOGW("setLayer: mutex timeout");
        return;
    }
    _layerManager.setLayer(layer, content);
    _layerVisibleTimeMs[static_cast<uint8_t>(layer)] = 0; // Reset visible time on new content
    LOGD("Layer %u set (type=%d, dur=%ums)", static_cast<unsigned>(layer), static_cast<int>(content.type), content.durationMs);
}

void MatrixManagerService::clearLayer(Layer layer) {
    SYSTEM::RecursiveScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        LOGW("clearLayer: mutex timeout");
        return;
    }
    _layerManager.clearLayer(layer);
    LOGD("Layer %u cleared", static_cast<unsigned>(layer));
}

bool MatrixManagerService::isLayerActive(Layer layer) const {
    SYSTEM::RecursiveScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return false;
    }
    return _layerManager.isLayerActive(layer);
}

// ─── Notification Queue API ──────────────────────────────────────

void MatrixManagerService::queueNotification(const char* text, uint32_t color, uint32_t displayMs) {
    SYSTEM::RecursiveScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        LOGW("queueNotification: mutex timeout");
        return;
    }
    Notification n;
    n.setText(text);
    n.color = color;
    n.displayMs = displayMs;
    const bool queuedWithoutDrop = _queue.push(n);
    if (!queuedWithoutDrop) {
        LOGW("Notification queue overflow - dropped oldest entry before queuing '%s'",
             text ? text : "null");
    }
    LOGI("Notification queued: '%s' (%ums)", text ? text : "null", displayMs);
}

uint8_t MatrixManagerService::pendingNotifications() const {
    SYSTEM::RecursiveScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        return 0;
    }
    return _queue.count();
}

// ─── Task Loop ───────────────────────────────────────────────────

void MatrixManagerService::update() {
    bool shouldApplyContent = false;
    bool shouldClearDisplay = false;
    LayerContent contentToApply;

    {
        SYSTEM::RecursiveScopeLock lock(_mutex);
        if (!lock.isLocked()) {
            LOGW("update: mutex timeout");
            return;
        }

        uint32_t now = millis();
        if (_lastUpdateMs == 0) _lastUpdateMs = now;
        uint32_t deltaMs = now - _lastUpdateMs;
        _lastUpdateMs = now;

        // Resolve the top layer first to know who gets the visible time tick
        LayerContent topContent;
        Layer topLayer;
        uint32_t topHash = 0;
        bool hasActiveLayer = _layerManager.getTopLayer(topContent, topLayer, topHash);

        if (hasActiveLayer) {
            _layerVisibleTimeMs[static_cast<uint8_t>(topLayer)] += deltaMs;
        }

        // Step 1: Advance notification queue timer
        advanceNotificationQueue();

        // Step 2: Auto-clear any expired layers
        checkLayerTimeouts();

        // The top layer might have changed due to timeouts, so resolve again
        hasActiveLayer = _layerManager.getTopLayer(topContent, topLayer, topHash);

        if (hasActiveLayer) {
            // Check if content changed. Layer hashes are computed on mutation.
            bool changed = !_wasRendering ||
                           topLayer != _lastRenderedLayer ||
                           topContent.type != _lastRenderedType ||
                           topHash != _lastContentHash;

            if (changed) {
                _lastRenderedContent = topContent;
                contentToApply = _lastRenderedContent;
                _lastRenderedLayer = topLayer;
                _lastRenderedType = topContent.type;
                _lastContentHash = topHash;
                _wasRendering = true;
                shouldApplyContent = true;
                LOGD("Rendering layer %u (type=%d)",
                     static_cast<unsigned>(topLayer), static_cast<int>(topContent.type));
            }
        } else {
            // No active layers — clear display
            if (_wasRendering) {
                _wasRendering = false;
                shouldClearDisplay = true;
                LOGD("All layers inactive, display cleared");
            }
        }
    }

    if (shouldApplyContent) {
        applyLayerToRenderer(contentToApply);
    } else if (shouldClearDisplay) {
        _matrixService->clear(false); // Preserve background effect
    }
}

void MatrixManagerService::invalidateCache() {
    SYSTEM::RecursiveScopeLock lock(_mutex);
    if (!lock.isLocked()) {
        LOGW("invalidateCache: mutex timeout");
        return;
    }
    _lastContentHash = 0;
    _wasRendering = false;
}

// ─── Internal Helpers ────────────────────────────────────────────

void MatrixManagerService::checkLayerTimeouts() {
    uint32_t now = millis();
    // Check all layers except NOTIFICATION (which is handled by advanceNotificationQueue)
    for (uint8_t i = 0; i < LAYER_COUNT; ++i) {
        Layer layer = static_cast<Layer>(i);
        if (layer == Layer::NOTIFICATION) continue;
        
        LayerContent content;
        if (_layerManager.getLayerContent(layer, content)) {
            // Check if this layer has an automatic expiration
            if (content.durationMs > 0) {
                if (_layerVisibleTimeMs[i] >= content.durationMs) {
                    LOGD("Layer %u expired (dur=%ums, visible=%ums), auto-clearing", i, content.durationMs, _layerVisibleTimeMs[i]);
                    _layerManager.clearLayer(layer);
                    _layerVisibleTimeMs[i] = 0;
                }
            }
        }
    }
}

void MatrixManagerService::advanceNotificationQueue() {
    // If currently showing a notification, check timeout based on visible time
    if (_notifActive) {
        if (_notifDurationMs > 0 && _layerVisibleTimeMs[static_cast<uint8_t>(Layer::NOTIFICATION)] >= _notifDurationMs) {
            // Current notification expired
            _queue.pop();
            _notifActive = false;
            _layerManager.clearLayer(Layer::NOTIFICATION);
            LOGD("Notification expired, popped from queue");
        } else {
            return; // Still showing current notification
        }
    }

    // Try to activate next notification from queue
    Notification next;
    if (_queue.peek(next)) {
        LayerContent content;
        content.active = true;
        content.type = CommandType::SHOW_TEXT;
        strlcpy(content.text, next.text, sizeof(content.text));
        content.color = next.color;
        content.durationMs = next.displayMs;

        _layerManager.setLayer(Layer::NOTIFICATION, content);
        _layerVisibleTimeMs[static_cast<uint8_t>(Layer::NOTIFICATION)] = 0;
        _notifDurationMs = next.displayMs;
        _notifActive = true;
        LOGD("Activated notification: '%s' for %ums", next.text, next.displayMs);
    }
}

void MatrixManagerService::applyLayerToRenderer(const LayerContent& content) {
    switch (content.type) {
        case CommandType::SHOW_TEXT:
            // Duration=0 because the Manager handles timing, not MatrixService
            _matrixService->showText(content.text, content.color, 0);
            break;

        case CommandType::SHOW_ICON:
            _matrixService->showIcon(content.icon, 0);
            break;

        case CommandType::SHOW_SOLID:
            _matrixService->showSolidColor(content.color);
            break;

        case CommandType::SHOW_EFFECT:
            _matrixService->showEffect(
                content.effectMode, content.effectSpeed,
                content.effectColor, content.effectColor2,
                content.effectColor3, 0,
                content.effectEngine,
                content.effectReactivityProvider,
                content.effectReactivityGain);
            break;

        case CommandType::SHOW_DATA_VISUALIZATION:
            _matrixService->showDataVisualization(content.dataVisualizationConfig, 0);
            break;

        case CommandType::CLEAR:
            _matrixService->clear(false);
            break;

        default:
            break;
    }
}

} // namespace MATRIX_MANAGER
