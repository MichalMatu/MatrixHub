#pragma once

#include <cstring>

#include "../config/json/ConfigKeys.h"
#include "MatrixSettingsTypes.h"

namespace MATRIX {

class MatrixCustomIconsCodec {
public:
    static bool capture(MatrixCustomIconsState& snapshot) {
        for (size_t i = 0; i < kMatrixCustomIconCount; i++) {
            snapshot.has[i] = hasCustomIcon(i);
            if (snapshot.has[i] && !copyCustomIcon(i, snapshot.icons[i])) {
                return false;
            }
        }

        return true;
    }

    static bool restore(const MatrixCustomIconsState& snapshot) {
        for (size_t i = 0; i < kMatrixCustomIconCount; i++) {
            const bool ok = snapshot.has[i] ? setCustomIcon(i, snapshot.icons[i])
                                            : clearCustomIcon(i);
            if (!ok) {
                return false;
            }
        }

        return true;
    }

    static bool equal(
        const MatrixCustomIconsState& lhs,
        const MatrixCustomIconsState& rhs) {
        for (size_t i = 0; i < kMatrixCustomIconCount; i++) {
            if (lhs.has[i] != rhs.has[i]) {
                return false;
            }
            if (!lhs.has[i]) {
                continue;
            }
            if (memcmp(lhs.icons[i], rhs.icons[i], sizeof(lhs.icons[i])) != 0) {
                return false;
            }
        }

        return true;
    }

    static void serialize(const MatrixCustomIconsState& state, JsonObject& root) {
        JsonArray icons = root[CONFIG::Keys::kCustomIcons].to<JsonArray>();
        for (size_t i = 0; i < kMatrixCustomIconCount; i++) {
            JsonArray icon = icons.add<JsonArray>();
            if (!state.has[i]) {
                continue;
            }

            for (size_t pixel = 0; pixel < kMatrixCustomIconPixels; pixel++) {
                icon.add(state.icons[i][pixel]);
            }
        }
    }

    static void deserialize(JsonObject& root, MatrixCustomIconsState& state) {
        if (!root[CONFIG::Keys::kCustomIcons].is<JsonArray>()) {
            return;
        }

        JsonArray icons = root[CONFIG::Keys::kCustomIcons].as<JsonArray>();
        for (size_t i = 0; i < icons.size() && i < kMatrixCustomIconCount; i++) {
            if (!icons[i].is<JsonArray>()) {
                continue;
            }

            JsonArray iconData = icons[i].as<JsonArray>();
            if (iconData.size() == 0) {
                memset(state.icons[i], 0, sizeof(state.icons[i]));
                state.has[i] = false;
                continue;
            }

            if (iconData.size() != kMatrixCustomIconPixels) {
                continue;
            }

            for (size_t pixel = 0; pixel < kMatrixCustomIconPixels; pixel++) {
                state.icons[i][pixel] = normalizeMatrixColor(iconData[pixel].as<uint32_t>());
            }
            state.has[i] = true;
        }
    }
};

}  // namespace MATRIX
