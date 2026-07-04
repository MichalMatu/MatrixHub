#include "AnsiFilter.h"
#include <cctype>

namespace USB_TERMINAL {

AnsiFilter::AnsiFilter() : _inAnsiSequence(false), _ansiSequenceLen(0) {}

void AnsiFilter::reset() {
    _inAnsiSequence = false;
    _ansiSequenceLen = 0;
}

bool AnsiFilter::processChar(char c) {
    if (c == '\x1B') {
        _inAnsiSequence = true;
        _ansiSequenceLen = 0;
        return false;
    }
    
    if (_inAnsiSequence) {
        _ansiSequenceLen++;
        // ANSI escape sequences end with letters (a-z, A-Z) or if it's too long
        if (isalpha(static_cast<unsigned char>(c)) || _ansiSequenceLen > LIMITS::USB_TERMINAL::MAX_ANSI_SEQ_LEN) {
            _inAnsiSequence = false;
        }
        return false;
    }
    
    return true; // Character is not part of an ANSI sequence
}

} // namespace USB_TERMINAL
