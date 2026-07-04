#include "IconDrawer.h"
#include <esp_log.h>

static const char* TAG = "IconDrawer";

void IconDrawer::draw(LedMatrix* matrix, IconType icon, const uint32_t* customBitmap) {
    if (!matrix) return;

    ESP_LOGD(TAG, "Drawing icon: %d %s", (int)icon, customBitmap ? "(Custom)" : "");
    
    const uint32_t* bitmap = customBitmap;

    if (!bitmap) {
        switch (icon) {
            case IconType::NONE:
                matrix->fillScreen(0);
                return;
            case IconType::ALARM_INFO:
                bitmap = MATRIX_ICONS::INFO;
                break;
            case IconType::ALARM_WARNING:
                bitmap = MATRIX_ICONS::WARNING;
                break;
            case IconType::ALARM_CRITICAL:
                bitmap = MATRIX_ICONS::CRITICAL;
                break;
        }
    }

    if (bitmap) {
        matrix->drawBitmap(bitmap);
    } else {
        matrix->fillScreen(0);
    }
}
