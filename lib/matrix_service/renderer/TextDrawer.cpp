#include "TextDrawer.h"
#include <string.h>

// Use Font5x7 constants from LedMatrix's font
#include <Font5x7.h>

int TextDrawer::getCharWidth(char c) {
    // Fixed width font (5 pixels + 1 spacing)
    (void)c; // Unused - fixed width
    return Font5x7::GLYPH_WIDTH;
}

int TextDrawer::getStringWidth(const char* str) {
    return LedMatrix::getStringWidth(str);
}
