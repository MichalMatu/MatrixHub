#include "filemanager/infrastructure/listing/DirectoryLister.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "system/watchdog/TaskWatchdog.h"

#include "filemanager/infrastructure/support/FileManagerPathUtils.h"
#include "system/utils/ScopeLock.h"
#include "config/Network.h"
#include "config/App.h"

using namespace FILEMGR;

namespace {

void toLowerInPlace(SYSTEM::PsramString& value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
}

}  // namespace

DirectoryLister::DirectoryLister(FS* fs, const char* requestedPath, const char* nativePath, SemaphoreHandle_t fsMutex)
  : _fs(fs),
    _requestedPath(SYSTEM::makePsramString((requestedPath && *requestedPath) ? requestedPath : "/")),
    _nativePath(SYSTEM::makePsramString((nativePath && *nativePath) ? nativePath : "/")),
    _fsMutex(fsMutex) {
  _normalizedRequestDir = FileManagerPathUtils::ensureAbsoluteDirectory(_requestedPath.c_str(), "/");
}

DirectoryLister::PopulateStatus DirectoryLister::prepare() {
  if (!_fs) {
    return PopulateStatus::OpenFailed;
  }

  _preparedEntries.clear();
  _preparedEntries.reserve(CONFIG::FILEMGR::kDirListReserve);
  _preparedIsDirectory = false;
  _preparedFileSize = 0;

  File dir;
  {
    SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) return PopulateStatus::FilesystemBusy;
    if (!_fs->exists(_nativePath.c_str())) return PopulateStatus::FileNotFound;
    dir = _fs->open(_nativePath.c_str(), "r");
    if (!dir) return PopulateStatus::OpenFailed;
    _preparedIsDirectory = dir.isDirectory();
    if (!_preparedIsDirectory) _preparedFileSize = static_cast<uint32_t>(dir.size());
  }

  PopulateStatus status = PopulateStatus::Success;

  if (_preparedIsDirectory) {
    status = handleDirectory(dir);
  }

  {
    SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) dir.close();
  }
  return status;
}

bool DirectoryLister::emitPrepared(Utils::JsonResponseWriter& w) const {
  if (_preparedIsDirectory) {
    emitEntries(_preparedEntries, w);
  } else {
    emitSingleFile(_preparedFileSize, w);
  }
  return true;
}

DirectoryLister::PopulateStatus DirectoryLister::handleDirectory(File& dir) {
  const PopulateStatus status = collectEntries(dir, _preparedEntries);
  if (status != PopulateStatus::Success) {
    return status;
  }
  sortEntries(_preparedEntries);
  return PopulateStatus::Success;
}

DirectoryLister::PopulateStatus DirectoryLister::collectEntries(File& dir, DirectoryEntryList& entries) const {
  while (true) {
    File entry;
    SYSTEM::PsramString rawName;
    uint32_t size = 0;
    bool isDir = false;
    
    {
      SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
      if (!lock.isLocked()) return PopulateStatus::FilesystemBusy;
      entry = dir.openNextFile();
      if (!entry) break;
      const char* entryName = entry.name();
      rawName = SYSTEM::makePsramString(entryName ? entryName : "");
      isDir = entry.isDirectory();
      size = isDir ? 0 : static_cast<uint32_t>(entry.size());
      entry.close();
    }

    const SYSTEM::PsramString relative = computeRelativePath(rawName.c_str());
    SYSTEM::PsramString fullPath = FileManagerPathUtils::buildFullPath(_normalizedRequestDir.c_str(), relative.c_str());
    SYSTEM::PsramString nativeFullPath = FileManagerPathUtils::buildFullPath(_nativePath.c_str(), relative.c_str());

    if (fullPath.empty() || nativeFullPath.empty()) {
      continue;
    }

    if (!FileManagerPathUtils::isAccessAllowed(
            nativeFullPath.c_str(), FileManagerPathUtils::FileManagerPathAccess::List)) {
      continue;
    }

    DirectoryEntry info;
    info.fullPath = std::move(fullPath);
    info.size = size;
    info.isDirectory = isDir;
    info.sortKey = info.fullPath;
    toLowerInPlace(info.sortKey);
    entries.emplace_back(std::move(info));

    vTaskDelay(pdMS_TO_TICKS(CONFIG::FILEMGR::kDirListYieldMs));  // Yield to prevent TWDT on large directories
    SYSTEM::TaskWatchdog::instance().reset();
  }

  return PopulateStatus::Success;
}

void DirectoryLister::sortEntries(DirectoryEntryList& entries) {
  std::sort(entries.begin(), entries.end(), [](const DirectoryEntry& lhs, const DirectoryEntry& rhs) {
    if (lhs.isDirectory != rhs.isDirectory) {
      return lhs.isDirectory && !rhs.isDirectory;
    }
    return lhs.sortKey.compare(rhs.sortKey) < 0;
  });
}

void DirectoryLister::emitEntries(const DirectoryEntryList& entries, Utils::JsonResponseWriter& w) const {
  bool first = true;
  for (const DirectoryEntry& entry : entries) {
    if (!first) w.raw(",");
    first = false;

    w.raw("{");
    w.key(CONFIG::Keys::kName); w.string(entry.fullPath.c_str()); w.raw(",");
    w.key(CONFIG::Keys::kFmSize); w.value(entry.size); w.raw(",");
    w.key(CONFIG::Keys::kFmDirectory); w.value(entry.isDirectory);
    w.raw("}");

    SYSTEM::TaskWatchdog::instance().reset();
  }
}

void DirectoryLister::emitSingleFile(uint32_t fileSize, Utils::JsonResponseWriter& w) const {
  w.raw("{");
  w.key(CONFIG::Keys::kName); w.string(_requestedPath.c_str()); w.raw(",");
  w.key(CONFIG::Keys::kFmSize); w.value(fileSize); w.raw(",");
  w.key(CONFIG::Keys::kFmDirectory); w.value(false);
  w.raw("}");
}

SYSTEM::PsramString DirectoryLister::computeRelativePath(const char* rawName) const {
  SYSTEM::PsramString relative = SYSTEM::makePsramString(rawName ? rawName : "");
  SYSTEM::PsramString prefix = _nativePath;

  while (prefix.size() > 1 && prefix.back() == '/') {
    prefix.pop_back();
  }

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

  return relative;
}
