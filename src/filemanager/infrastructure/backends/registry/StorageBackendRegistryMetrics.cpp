#include "filemanager/infrastructure/backends/registry/StorageBackendRegistry.h"

#include <LittleFS.h>

using namespace FILEMGR;
using SYSTEM::makePsramString;
using SYSTEM::PsramString;

StorageMetrics StorageBackendRegistry::getLittlefsMetrics() const
{
    StorageMetrics metrics;
    metrics.backendId = makePsramString(BACKEND_LITTLEFS);
    metrics.available = _littleFs != nullptr;
    metrics.mounted = _littleFs != nullptr;
    if (metrics.available)
    {
        metrics.totalBytes = LittleFS.totalBytes();
        metrics.usedBytes = LittleFS.usedBytes();
    }
    else
    {
        metrics.totalBytes = 0;
        metrics.usedBytes = 0;
    }
    metrics.lastError = PsramString();
    return metrics;
}

StorageMetrics StorageBackendRegistry::getMetricsForPath(const SYSTEM::PsramString& path) const
{
    (void)path;
    // SD card is not supported on this hardware
    return getLittlefsMetrics();
}
