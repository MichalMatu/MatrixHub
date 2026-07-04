#pragma once

#include <FS.h>

#include "core/config/ConfigManager.h"
#include "../system/rtc/RtcConfig.h"
#include "MatrixCustomIconsCodec.h"

namespace MATRIX {

class MatrixSettingsStore {
public:
    explicit MatrixSettingsStore(fs::FS* fs)
        : _fs(fs) {}

    bool sync(MatrixSettingsState& state) const {
        bool synced = false;
        RTC::withConfig([&](const RTC::ConfigStore& cfg) {
            state.config = cfg.matrix;
            synced = true;
        });
        return synced && MatrixCustomIconsCodec::capture(state.customIcons);
    }

    bool write(const MatrixSettingsState& state) const {
        if (!RTC::updateConfig([&](RTC::ConfigStore& cfg) {
                cfg.matrix = state.config;
            })) {
            return false;
        }

        return MatrixCustomIconsCodec::restore(state.customIcons);
    }

    bool persist(const MatrixSettingsState& state) const {
        (void)state;
        return _fs && CONFIG::save(*_fs);
    }

private:
    fs::FS* _fs = nullptr;
};

}  // namespace MATRIX
