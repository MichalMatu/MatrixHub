#pragma once

#include <Arduino.h>
#include <LedMatrix.h>

class TextDrawer {
public:
    /**
     * @brief Get width of a single character in pixels
     */
    static int getCharWidth(char c);

    /**
     * @brief Get total pixel width of a string including spacing
     */
    static int getStringWidth(const char* str);
};
