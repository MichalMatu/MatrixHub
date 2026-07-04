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

    if (config.backgroundMode == static_cast<uint8_t>(MatrixBackgroundMode::DataVisualization)) {
        config.effectEnabled = false;
    } else {
        config.dataVisualizationEnabled = false;
    }
}

}  // namespace MATRIX
