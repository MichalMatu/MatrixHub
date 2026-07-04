#pragma once

#include <Arduino.h>

#include "system/memory/PsramAllocator.h"

namespace FILEMGR {

struct StorageMetrics {
    SYSTEM::PsramString backendId;
    bool available;
    bool mounted;
    size_t totalBytes;
    size_t usedBytes;
    SYSTEM::PsramString lastError;

    size_t freeBytes() const { return (totalBytes > usedBytes) ? (totalBytes - usedBytes) : 0; }
};

}  // namespace FILEMGR
