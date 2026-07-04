#pragma once

#include <cstdint>
#include "../../config/System.h"

namespace USB_TERMINAL {

/**
 * @class AnsiFilter
 * @brief Filter state machine for ignoring ANSI escape sequences in the terminal output.
 */
class AnsiFilter {
public:
    AnsiFilter();
    
    /**
     * @brief Process a single character.
     * @param c Character to process
     * @return true if character is a valid output character (not part of ANSI sequence)
     * @return false if character is part of an ANSI sequence and should be ignored
     */
    bool processChar(char c);

    /**
     * @brief Reset the state machine, typically called when a new capture starts.
     */
    void reset();

private:
    bool _inAnsiSequence;
    unsigned int _ansiSequenceLen;
};

} // namespace USB_TERMINAL
