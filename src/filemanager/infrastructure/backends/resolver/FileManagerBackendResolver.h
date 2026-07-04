#pragma once

#include <Arduino.h>
#include <FS.h>
#include "system/memory/PsramAllocator.h"

namespace FILEMGR {

class StorageService;

struct FileManagerBackendResolution {
  FS* filesystem = nullptr;
  SYSTEM::PsramString nativePath;
};

class FileManagerBackendResolver {
 public:
  explicit FileManagerBackendResolver(StorageService* storage);

  FileManagerBackendResolution resolveForPath(const SYSTEM::PsramString& path) const;
  FileManagerBackendResolution resolveForUpload(const SYSTEM::PsramString& requestedPath) const;

 private:
  SYSTEM::PsramString normalizeUploadDirectory(const SYSTEM::PsramString& requestedPath) const;

  StorageService* _storage;
};

}  // namespace FILEMGR
