#pragma once

#include <FS.h>
#include <Arduino.h>
#include <freertos/semphr.h>

#include "system/memory/PsramAllocator.h"
#include "system/utils/json/JsonResponseWriter.h"

namespace FILEMGR {

class DirectoryLister {
public:
  enum class PopulateStatus : uint8_t {
    Success,
    FilesystemBusy,
    FileNotFound,
    OpenFailed,
  };

  DirectoryLister(FS* fs, const char* requestedPath, const char* nativePath, SemaphoreHandle_t fsMutex);

  PopulateStatus prepare();
  bool emitPrepared(Utils::JsonResponseWriter& w) const;

private:
  struct DirectoryEntry {
    SYSTEM::PsramString fullPath;
    SYSTEM::PsramString sortKey;
    uint32_t size;
    bool isDirectory;
  };

  using DirectoryEntryList = SYSTEM::PsramVector<DirectoryEntry>;

  PopulateStatus handleDirectory(File& dir);
  PopulateStatus collectEntries(File& dir, DirectoryEntryList& entries) const;
  static void sortEntries(DirectoryEntryList& entries);
  void emitEntries(const DirectoryEntryList& entries, Utils::JsonResponseWriter& w) const;
  void emitSingleFile(uint32_t fileSize, Utils::JsonResponseWriter& w) const;

  SYSTEM::PsramString computeRelativePath(const char* rawName) const;

  FS* _fs;
  SYSTEM::PsramString _requestedPath;
  SYSTEM::PsramString _normalizedRequestDir;
  SYSTEM::PsramString _nativePath;
  SemaphoreHandle_t _fsMutex;
  DirectoryEntryList _preparedEntries;
  bool _preparedIsDirectory = false;
  uint32_t _preparedFileSize = 0;
};

}  // namespace FILEMGR
