#pragma once

#include <MatrixService.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "../alarms/AlarmService.h"
#include "../system/matrix_manager/MatrixManagerService.h"
#include "MatrixDataVisualizationTypes.h"
#include "menu/MatrixMenuService.h"
#include "MatrixSettingsTypes.h"

namespace MATRIX {

class MatrixRuntimeApplier {
public:
    MatrixRuntimeApplier(
        MatrixService* matrixService,
        ALARMS::AlarmService* alarmService,
        MATRIX_MANAGER::MatrixManagerService* matrixManager,
        MatrixMenuService* menuService)
        : _matrixService(matrixService)
        , _alarmService(alarmService)
        , _matrixManager(matrixManager)
        , _menuService(menuService) {}

    void apply(const MatrixSettingsState& state) const {
        _matrixService->setBrightness(state.config.brightness);
        _matrixService->setScrollSpeed(state.config.menu.scrollSpeed);

        if (!state.config.autoRotate) {
            _matrixService->setRotation(state.config.rotation);
        }

        const bool wantsDataVisualization =
            state.config.backgroundMode == static_cast<uint8_t>(MatrixBackgroundMode::DataVisualization) &&
            state.config.dataVisualizationEnabled;
        const bool wantsEffects =
            state.config.backgroundMode == static_cast<uint8_t>(MatrixBackgroundMode::Effects) &&
            state.config.effectEnabled;

        if (wantsDataVisualization) {
            MatrixDataVisualizationConfig config;
            config.enabled = state.config.dataVisualizationEnabled;
            config.source = state.config.dataVisualizationSource;
            config.metric = state.config.dataVisualizationMetric;
            config.mode = state.config.dataVisualizationMode;
            config.minValue = state.config.dataVisualizationMin;
            config.maxValue = state.config.dataVisualizationMax;
            config.colorMin = state.config.dataVisualizationColorMin;
            config.colorMid = state.config.dataVisualizationColorMid;
            config.colorMax = state.config.dataVisualizationColorMax;
            config.brightnessMin = state.config.dataVisualizationBrightnessMin;
            config.brightnessMax = state.config.dataVisualizationBrightnessMax;
            config.smoothing = state.config.dataVisualizationSmoothing;
            config.staleBehavior = state.config.dataVisualizationStaleBehavior;
            copyMatrixDataDeviceId(config.deviceId, sizeof(config.deviceId), state.config.dataVisualizationDeviceId);
            normalizeMatrixDataVisualizationConfig(config);

            if (_matrixManager) {
                MATRIX_MANAGER::LayerContent bgContent;
                bgContent.active = true;
                bgContent.type = CommandType::SHOW_DATA_VISUALIZATION;
                bgContent.dataVisualizationConfig = config;
                _matrixManager->setLayer(MATRIX_MANAGER::Layer::BACKGROUND, bgContent);
            } else {
                _matrixService->clearBackgroundEffect();
                _matrixService->showDataVisualization(config, 0);
            }
        } else if (wantsEffects) {
            if (_matrixManager) {
                // In layered mode the background effect must be published as the
                // BACKGROUND layer, not pushed directly to the renderer.
                MATRIX_MANAGER::LayerContent bgContent;
                bgContent.active = true;
                bgContent.type = CommandType::SHOW_EFFECT;
                bgContent.effectEngine = state.config.effectEngine;
                bgContent.effectMode = state.config.effectMode;
                bgContent.effectSpeed = state.config.effectSpeed;
                bgContent.effectColor = state.config.effectColor;
                bgContent.effectColor2 = state.config.effectColor2;
                bgContent.effectColor3 = state.config.effectColor3;
                bgContent.effectReactivityProvider = state.config.effectReactivityProvider;
                bgContent.effectReactivityGain = state.config.effectReactivityGain;
                _matrixManager->setLayer(MATRIX_MANAGER::Layer::BACKGROUND, bgContent);
            } else {
                _matrixService->clearBackgroundDataVisualization();
                _matrixService->showEffect(
                    state.config.effectMode,
                    state.config.effectSpeed,
                    state.config.effectColor,
                    state.config.effectColor2,
                    state.config.effectColor3,
                    0,
                    state.config.effectEngine,
                    state.config.effectReactivityProvider,
                    state.config.effectReactivityGain);
            }
        } else {
            // The renderer can resurrect a cached background effect after a
            // later clear(false), so disable the cached effect state too.
            // Regression note: clearing only Layer::BACKGROUND made the UI say
            // "effects disabled" while the old animation could still come back.
            _matrixService->clearBackgroundEffect();
            _matrixService->clearBackgroundDataVisualization();
            if (_matrixManager) {
                // Keep all visible-state decisions inside the layer manager.
                // Clearing the renderer directly here can blank higher layers
                // such as MENU until some later producer re-publishes content.
                _matrixManager->clearLayer(MATRIX_MANAGER::Layer::BACKGROUND);
            } else if (!_alarmService || !_alarmService->isAlarmLatched()) {
                _matrixService->clear(true);
            }
        }

        for (int i = 0; i < static_cast<int>(kMatrixCustomIconCount); i++) {
            const IconType type = static_cast<IconType>(i + 1);
            _matrixService->setCustomIcon(
                type,
                state.customIcons.has[i] ? state.customIcons.icons[i] : nullptr);
        }

        if (_alarmService) {
            vTaskDelay(pdMS_TO_TICKS(50));
            // Re-publish any latched alarm so display-mode changes do not leave
            // the matrix showing stale lower-priority content.
            _alarmService->reapplyLatchedState();
        }

        if (_menuService) {
            // Menu text/color comes from live formatted state, so after config
            // changes we invalidate its dedup cache and let the next menu tick
            // re-send the current screen with fresh styling.
            _menuService->invalidateCache();
        }
        if (_matrixManager) {
            // Force the manager to recompute the visible top layer after the
            // background/menu/alarm state above may have changed.
            _matrixManager->invalidateCache();
        }
    }

private:
    MatrixService* _matrixService = nullptr;
    ALARMS::AlarmService* _alarmService = nullptr;
    MATRIX_MANAGER::MatrixManagerService* _matrixManager = nullptr;
    MatrixMenuService* _menuService = nullptr;
};

}  // namespace MATRIX
