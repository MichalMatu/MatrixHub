#pragma once

#include <LedMatrix.h>
#include "../assets/MatrixBitmaps.h"
#include "../types/MatrixTypes.h"

class IconDrawer {
public:
    /**
     * @brief Render an icon to the matrix buffer
     * @param matrix Pointer to the LedMatrix object
     * @param icon Enum of the icon to draw
     * @param customBitmap Optional pointer to custom 8x8 bitmap (RGB888). Uses default if NULL.
     */
    static void draw(LedMatrix* matrix, IconType icon, const uint32_t* customBitmap = nullptr);
};
