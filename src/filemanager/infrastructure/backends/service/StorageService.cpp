#include "filemanager/infrastructure/backends/service/StorageService.h"

#include "filemanager/infrastructure/backends/registry/StorageBackendRegistry.h"
#include "filemanager/infrastructure/support/FileManagerPathUtils.h"

#include "system/logging/Logging.h"
#include "system/utils/ScopeLock.h"
#include "system/watchdog/TaskWatchdog.h"
#include <vector>
#undef LOG_TAG
#define LOG_TAG "FileMgr"

using namespace FILEMGR;

namespace {
constexpr const char* kDefaultLittlefsPath = "/data";

SYSTEM::PsramString normalizeChildEntryPath(const SYSTEM::PsramString& parentPath, const char* rawEntryName)
{
    SYSTEM::PsramString relative = SYSTEM::makePsramString(rawEntryName ? rawEntryName : "");
    SYSTEM::PsramString prefix = parentPath;

    while (prefix.size() > 1 && prefix.back() == '/') {
        prefix.pop_back();
    }

    // Production fix: recursive directory removal used to append entry.name()
    // directly to the parent path. On this stack openNextFile() may return
    // either "child.txt" or "/dir/child.txt", so the old code could build
    // malformed paths like "/dir//dir/child.txt" and silently fail to remove
    // nested directories. Normalize both forms into one canonical child path
    // before we recurse.
    if (!prefix.empty() && relative.rfind(prefix, 0) == 0) {
        relative.erase(0, prefix.size());
    }

    size_t start = 0;
    while (start < relative.size() && relative[start] == '/') {
        ++start;
    }
    if (start > 0) {
        relative.erase(0, start);
    }

    return FileManagerPathUtils::joinPaths(parentPath, relative);
}
}  // namespace

StorageService::StorageService(FS* littleFs)
    : _backendRegistry(littleFs),
      _defaultLittlefsPath(SYSTEM::makePsramString(kDefaultLittlefsPath))
{
}

void StorageService::begin()
{
    _backendRegistry.begin();
}

FS* StorageService::resolveFilesystem(const SYSTEM::PsramString& path)
{
    return _backendRegistry.resolveFilesystem(path);
}

bool StorageService::ensurePathExists(const SYSTEM::PsramString& path)
{
    SYSTEM::PsramString canonicalPath;
    if (!FileManagerPathUtils::canonicalizeAbsolutePath(path, SYSTEM::makePsramString("/"), canonicalPath))
    {
        return false;
    }

    FS* backend = resolveFilesystem(canonicalPath);
    if (!backend)
    {
        return false;
    }

    const size_t lastSlash = canonicalPath.find_last_of('/');
    if (lastSlash == SYSTEM::PsramString::npos || lastSlash == 0)
    {
        return true;
    }

    const SYSTEM::PsramString directory = canonicalPath.substr(0, lastSlash);
    return _backendRegistry.ensureDirectoryTree(backend, directory);
}

bool StorageService::ensureDirectoryTree(const SYSTEM::PsramString& directory)
{
    SYSTEM::PsramString canonicalDirectory;
    if (!FileManagerPathUtils::canonicalizeAbsoluteDirectory(
            directory, SYSTEM::makePsramString("/"), canonicalDirectory)) {
        return false;
    }

    FS* backend = resolveFilesystem(canonicalDirectory);
    if (!backend)
    {
        return false;
    }
    return _backendRegistry.ensureDirectoryTree(backend, canonicalDirectory);
}

StorageService::Metrics StorageService::getLittlefsMetrics() const
{
    return _backendRegistry.getLittlefsMetrics();
}

StorageService::Metrics StorageService::getMetricsForPath(const SYSTEM::PsramString& path) const
{
    return _backendRegistry.getMetricsForPath(path);
}

SYSTEM::PsramString StorageService::toFilesystemPath(const SYSTEM::PsramString& path) const
{
    return _backendRegistry.toFilesystemPath(path);
}

void StorageService::onSensorPathUpdated(const SYSTEM::PsramString& fsPath)
{
    _backendRegistry.onSensorPathUpdated(fsPath);
}

bool StorageService::removePath(
    const SYSTEM::PsramString& path,
    SemaphoreHandle_t fsMutex,
    TickType_t timeout
)
{
    SYSTEM::PsramString canonicalRequestedPath;
    if (!FileManagerPathUtils::canonicalizeAbsolutePath(
            path, SYSTEM::makePsramString("/"), canonicalRequestedPath)) {
        LOGW("StorageService: Refusing to remove invalid path: %s", path.c_str());
        return false;
    }

    FS* fs = resolveFilesystem(canonicalRequestedPath);
    if (!fs) return false;

    SYSTEM::PsramString nativePath = toFilesystemPath(canonicalRequestedPath);
    if (nativePath.empty()) {
        LOGW("StorageService: Unable to resolve native path for: %s", canonicalRequestedPath.c_str());
        return false;
    }

    if (!FileManagerPathUtils::isAccessAllowed(nativePath, FileManagerPathUtils::FileManagerPathAccess::Remove)) {
        LOGW("StorageService: Refusing to remove protected path: request=%s native=%s",
             canonicalRequestedPath.c_str(), nativePath.c_str());
        return false;
    }

    bool ok = false;
    bool isDir = false;

    {
        SYSTEM::ScopeLock lock(fsMutex, timeout);
        if (!lock.isLocked()) {
            LOGW("FS mutex timeout on remove: %s", canonicalRequestedPath.c_str());
            return false;
        }
        File f = fs->open(nativePath.c_str());
        if (f) {
            isDir = f.isDirectory();
            f.close();
        }
    }

    if (isDir) {
        {
            SYSTEM::ScopeLock lock(fsMutex, timeout);
            if (lock.isLocked()) ok = fs->rmdir(nativePath.c_str());
        }
        if (!ok) {
            // Fast path keeps simple empty-directory deletes cheap. The
            // recursive fallback is only for non-empty directories.
            ok = removeDirectoryRecursive(fs, nativePath, fsMutex, timeout);
        }
    } else {
        {
            SYSTEM::ScopeLock lock(fsMutex, timeout);
            if (lock.isLocked()) ok = fs->remove(nativePath.c_str());
        }
    }

    return ok;
}

bool StorageService::removeDirectoryRecursive(
    FS* fs,
    const SYSTEM::PsramString& path,
    SemaphoreHandle_t fsMutex,
    TickType_t timeout
)
{
    if (!fs) return false;

    File dir;
    {
        SYSTEM::ScopeLock lock(fsMutex, timeout);
        if (!lock.isLocked()) return false;
        if (!fs->exists(path.c_str())) return false;
        dir = fs->open(path.c_str());
        if (!dir || !dir.isDirectory()) {
            if (dir) dir.close();
            return fs->remove(path.c_str());
        }
    }

    // Collect child paths first, then delete after closing the directory
    // handle. This keeps the FS critical section smaller and avoids mutating
    // the directory while we are still iterating it.
    std::vector<SYSTEM::PsramString, SYSTEM::PsramAllocator<SYSTEM::PsramString>> entriesToDelete;
    std::vector<bool> isDirList;
    bool collectionFailed = false;

    while (true) {
        File entry;
        SYSTEM::PsramString entryPath;
        bool isDir = false;

        {
            SYSTEM::ScopeLock lock(fsMutex, timeout);
            if (!lock.isLocked()) break;
            entry = dir.openNextFile();
            if (!entry) break;

            entryPath = normalizeChildEntryPath(path, entry.name());
            isDir = entry.isDirectory();
            entry.close();
        }

        if (entryPath.empty()) {
            // Refuse to descend when the backend reports a child entry we
            // cannot normalize back to a safe canonical path. Returning false
            // here is safer than guessing and risking partial deletion.
            collectionFailed = true;
            break;
        }

        entriesToDelete.push_back(entryPath);
        isDirList.push_back(isDir);

        vTaskDelay(1);
        SYSTEM::TaskWatchdog::instance().reset();
    }

    {
        SYSTEM::ScopeLock lock(fsMutex, timeout);
        if (lock.isLocked()) dir.close();
    }

    if (collectionFailed) {
        return false;
    }

    bool allOk = true;
    for (size_t i = 0; i < entriesToDelete.size(); ++i) {
        bool childOk;
        if (isDirList[i]) {
            childOk = removeDirectoryRecursive(fs, entriesToDelete[i], fsMutex, timeout);
        } else {
            SYSTEM::ScopeLock lock(fsMutex, timeout);
            if (lock.isLocked()) childOk = fs->remove(entriesToDelete[i].c_str());
            else childOk = false;
        }
        if (!childOk) allOk = false;
        
        vTaskDelay(1);
        SYSTEM::TaskWatchdog::instance().reset();
    }

    {
        SYSTEM::ScopeLock lock(fsMutex, timeout);
        if (lock.isLocked()) {
            if (!allOk) return false;
            return fs->rmdir(path.c_str());
        }
    }
    return false;
}
