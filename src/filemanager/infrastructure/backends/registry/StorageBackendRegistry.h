#pragma once

#include <Arduino.h>
#include <FS.h>

#include "system/memory/PsramAllocator.h"

#include "filemanager/infrastructure/backends/common/StorageMetrics.h"

namespace FILEMGR {

class StorageBackendRegistry {
public:
    static constexpr const char* BACKEND_LITTLEFS = "littlefs";

    StorageBackendRegistry(FS* littleFs);

    void begin();

    FS* resolveFilesystem(const SYSTEM::PsramString& path);

    StorageMetrics getLittlefsMetrics() const;
    StorageMetrics getMetricsForPath(const SYSTEM::PsramString& path) const;

    void onSensorPathUpdated(const SYSTEM::PsramString& fsPath);

    const SYSTEM::PsramString& getActiveBackend() const { return _activeBackend; }
    const SYSTEM::PsramString& getActivePath() const { return _activePath; }

    SYSTEM::PsramString toFilesystemPath(const SYSTEM::PsramString& path) const;

    bool ensureDirectoryTree(FS* fs, const SYSTEM::PsramString& directory) const;

private:
    bool isSdPath(const SYSTEM::PsramString& path) const;

    FS* _littleFs;
    SYSTEM::PsramString _activeBackend;
    SYSTEM::PsramString _activePath;
};

}  // namespace FILEMGR
