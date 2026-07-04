#include "filemanager/infrastructure/backends/resolver/FileManagerBackendResolver.h"

#include <LittleFS.h>
#include "filemanager/infrastructure/support/FileManagerPathUtils.h"
#include <filemanager/infrastructure/backends/service/StorageService.h>

using namespace FILEMGR;

FileManagerBackendResolver::FileManagerBackendResolver(StorageService* storage)
  : _storage(storage) {}

FileManagerBackendResolution FileManagerBackendResolver::resolveForPath(
    const SYSTEM::PsramString& path
) const {
  FileManagerBackendResolution resolution;

  SYSTEM::PsramString canonicalPath;
  if (!FileManagerPathUtils::canonicalizeAbsolutePath(path, SYSTEM::makePsramString("/"), canonicalPath)) {
    return resolution;
  }

  if (!_storage) {
    resolution.filesystem = &LittleFS;
    resolution.nativePath = canonicalPath;
    return resolution;
  }

  FS* backend = _storage->resolveFilesystem(canonicalPath);
  if (!backend) {
    return resolution;
  }

  const SYSTEM::PsramString nativePath = _storage->toFilesystemPath(canonicalPath);
  if (nativePath.empty()) {
    return resolution;
  }

  resolution.filesystem = backend;
  resolution.nativePath = nativePath;
  return resolution;
}

FileManagerBackendResolution FileManagerBackendResolver::resolveForUpload(
    const SYSTEM::PsramString& requestedPath
) const {
  const SYSTEM::PsramString targetDirectory = normalizeUploadDirectory(requestedPath);
  if (targetDirectory.empty()) {
    return FileManagerBackendResolution();
  }
  return resolveForPath(targetDirectory);
}

SYSTEM::PsramString FileManagerBackendResolver::normalizeUploadDirectory(
    const SYSTEM::PsramString& requestedPath
) const {
  SYSTEM::PsramString canonicalPath;
  if (!FileManagerPathUtils::canonicalizeAbsoluteDirectory(
          requestedPath, SYSTEM::makePsramString("/"), canonicalPath)) {
    return SYSTEM::PsramString();
  }
  return canonicalPath;
}
