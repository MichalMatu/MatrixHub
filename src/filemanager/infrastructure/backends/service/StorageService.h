#pragma once

#include <Arduino.h>
#include <FS.h>

#include "filemanager/infrastructure/backends/registry/StorageBackendRegistry.h"
#include "filemanager/infrastructure/backends/common/StorageMetrics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace FILEMGR {

/**
 * StorageService coordinates access to the built-in LittleFS volume.
 */
class StorageService {
public:
    using Metrics = StorageMetrics;

    explicit StorageService(FS* littleFs);

    void begin();

    /**
     * Resolve the filesystem backing the supplied absolute path.
     * Returns nullptr when the requested backend is unavailable.
     */
    FS* resolveFilesystem(const SYSTEM::PsramString& path);

    /**
     * Ensure the parent directories for a given path exist on the appropriate
     * backend.
     */
    bool ensurePathExists(const SYSTEM::PsramString& path);

    /**
     * Ensure an absolute directory path exists on the appropriate backend.
     */
    bool ensureDirectoryTree(const SYSTEM::PsramString& directory);

    Metrics getLittlefsMetrics() const;
    Metrics getMetricsForPath(const SYSTEM::PsramString& path) const;

    const SYSTEM::PsramString& getActiveBackend() const { return _backendRegistry.getActiveBackend(); }
    const SYSTEM::PsramString& getActivePath() const { return _backendRegistry.getActivePath(); }
    const SYSTEM::PsramString& defaultLittlefsPath() const { return _defaultLittlefsPath; }

    /**
     * Translate an absolute path into the form expected
     * by the underlying Arduino FS implementation.
     */
    SYSTEM::PsramString toFilesystemPath(const SYSTEM::PsramString& path) const;

    void onSensorPathUpdated(const SYSTEM::PsramString& fsPath);

    /**
     * Remove a file or directory recursively.
     * @param path Absolute path to remove
     * @param fsMutex Mutex to protect FS access
     * @param timeout Maximum time to wait for the mutex
     * @return true if successful
     */
    bool removePath(const SYSTEM::PsramString& path, SemaphoreHandle_t fsMutex, TickType_t timeout);

private:
    bool removeDirectoryRecursive(
        FS* fs,
        const SYSTEM::PsramString& path,
        SemaphoreHandle_t fsMutex,
        TickType_t timeout
    );

    StorageBackendRegistry _backendRegistry;
    SYSTEM::PsramString _defaultLittlefsPath;
};

}  // namespace FILEMGR
