#pragma once

#include "../system/rtc/RtcConfig.h"
#include "MatrixCustomIconStore.h"
#include "MatrixDataVisualizationTypes.h"

namespace MATRIX {

struct MatrixCustomIconsState {
    bool has[kMatrixCustomIconCount] = {false};
    uint32_t icons[kMatrixCustomIconCount][kMatrixCustomIconPixels] = {};
};

struct MatrixSettingsState {
    RTC::MatrixData config{};
    MatrixCustomIconsState customIcons{};
};

inline void normalizeMatrixBackgroundSelection(RTC::MatrixData& config) {
    config.backgroundMode = normalizeMatrixBackgroundMode(config.backgroundMode);

    if (!config.effectEnabled && !config.dataVisualizationEnabled) {
        config.backgroundMode = static_cast<uint8_t>(MatrixBackgroundMode::Off);
        return;
    }

    switch (static_cast<MatrixBackgroundMode>(config.backgroundMode)) {
        case MatrixBackgroundMode::Effects:
            if (config.effectEnabled) {
                config.dataVisualizationEnabled = false;
            } else if (config.dataVisualizationEnabled) {
                config.backgroundMode = static_cast<uint8_t>(MatrixBackgroundMode::DataVisualization);
            } else {
                config.backgroundMode = static_cast<uint8_t>(MatrixBackgroundMode::Off);
            }
            break;

        case MatrixBackgroundMode::DataVisualization:
            if (config.dataVisualizationEnabled) {
                config.effectEnabled = false;
            } else if (config.effectEnabled) {
                config.backgroundMode = static_cast<uint8_t>(MatrixBackgroundMode::Effects);
            } else {
                config.backgroundMode = static_cast<uint8_t>(MatrixBackgroundMode::Off);
            }
            break;

        case MatrixBackgroundMode::Off:
        default:
            config.effectEnabled = false;
            config.dataVisualizationEnabled = false;
            break;
    }
}

}  // namespace MATRIX
