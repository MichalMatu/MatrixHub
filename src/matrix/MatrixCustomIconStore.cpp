#include "MatrixCustomIconStore.h"

#include <esp_attr.h>
#include <cstring>

#include "../system/logging/Logging.h"
#include "../system/rtc/RtcConfig.h"
#include "../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "MatrixIcons"

namespace MATRIX {
namespace {

struct MatrixCustomIconCache {
    uint32_t icons[kMatrixCustomIconCount][kMatrixCustomIconPixels]{};
    bool has[kMatrixCustomIconCount]{};
};

EXT_RAM_BSS_ATTR MatrixCustomIconCache s_iconCache;
constexpr TickType_t kIconStoreLockTimeout = pdMS_TO_TICKS(100);

template <typename Fn>
bool withIconStoreLock(Fn&& fn) {
    SemaphoreHandle_t lock = RTC::getLock();
    if (!lock) {
        LOGW("Custom icon store access before RTC lock initialization");
        return false;
    }

    SYSTEM::ScopeLock guard(lock, kIconStoreLockTimeout);
    if (!guard.isLocked()) {
        LOGW("Custom icon store lock timeout");
        return false;
    }

    fn();
    return true;
}

bool isValidIndex(size_t index) {
    return index < kMatrixCustomIconCount;
}

}  // namespace

bool hasCustomIcon(size_t index) {
    if (!isValidIndex(index)) {
        return false;
    }

    bool hasIcon = false;
    withIconStoreLock([&]() {
        hasIcon = s_iconCache.has[index];
    });
    return hasIcon;
}

bool copyCustomIcon(size_t index, uint32_t* outBuffer, size_t pixelCount) {
    if (!isValidIndex(index) || !outBuffer || pixelCount != kMatrixCustomIconPixels) {
        return false;
    }

    bool hasIcon = false;
    return withIconStoreLock([&]() {
        hasIcon = s_iconCache.has[index];
        if (hasIcon) {
            memcpy(outBuffer, s_iconCache.icons[index], sizeof(s_iconCache.icons[index]));
        }
    }) && hasIcon;
}

bool setCustomIcon(size_t index, const uint32_t* bitmap, size_t pixelCount) {
    if (!isValidIndex(index) || pixelCount != kMatrixCustomIconPixels) {
        return false;
    }

    return withIconStoreLock([&]() {
        if (bitmap) {
            for (size_t pixel = 0; pixel < kMatrixCustomIconPixels; ++pixel) {
                s_iconCache.icons[index][pixel] = normalizeMatrixColor(bitmap[pixel]);
            }
            s_iconCache.has[index] = true;
        } else {
            memset(s_iconCache.icons[index], 0, sizeof(s_iconCache.icons[index]));
            s_iconCache.has[index] = false;
        }
    });
}

bool clearCustomIcon(size_t index) {
    return setCustomIcon(index, nullptr);
}

void clearAllCustomIcons() {
    withIconStoreLock([&]() {
        memset(&s_iconCache, 0, sizeof(s_iconCache));
    });
}

bool customIconEquals(size_t index, const uint32_t* bitmap, size_t pixelCount) {
    if (!isValidIndex(index) || pixelCount != kMatrixCustomIconPixels) {
        return false;
    }

    bool equals = false;
    withIconStoreLock([&]() {
        if (!s_iconCache.has[index]) {
            equals = false;
            return;
        }
        equals = bitmap && memcmp(s_iconCache.icons[index], bitmap, sizeof(s_iconCache.icons[index])) == 0;
    });
    return equals;
}

}  // namespace MATRIX
