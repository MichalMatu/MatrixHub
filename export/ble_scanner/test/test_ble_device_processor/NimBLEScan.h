#pragma once
#include "NimBLEDevice.h"

class NimBLEScan {
public:
    void setCallbacks(NimBLEScanCallbacks* cb) {}
    bool start(uint32_t duration, bool is_continual) { return true; }
    void stop() {}
};
