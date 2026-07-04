#include "filemanager/infrastructure/uploads/UploadSession.h"

#include "filemanager/infrastructure/support/FileManagerPathUtils.h"
#include "filemanager/infrastructure/uploads/UploadHooks.h"
#include "system/utils/ScopeLock.h"
#include "config/Network.h"
#include "config/App.h"

#include "system/logging/Logging.h"
#undef LOG_TAG
#define LOG_TAG "FileMgr"

using namespace FILEMGR;

UploadSession::UploadSession(FS* fs, const char* nativeDirectory, SemaphoreHandle_t fsMutex, size_t maxFileSize)
  : _fs(fs),
    _nativeDirectory(SYSTEM::makePsramString(nativeDirectory ? nativeDirectory : "")),
    _fsMutex(fsMutex),
    _totalBytesWritten(0),
    _maxFileSize(maxFileSize),
    _lastActivityMs(millis()),
    _isSuccess(false) {}

UploadSession::~UploadSession() {
    closeFile();

    // ERROR HANDLING / GARBAGE COLLECTION:
    // If destructor is called and session didn't end with success,
    // it means a failure (timeout, OOM, disconnect).
    // WE MUST REMOVE THE .TMP FILE!
    if (!_isSuccess && _fs && !_tmpFilePath.empty()) {
        SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (lock.isLocked() && _fs->exists(_tmpFilePath.c_str())) {
            _fs->remove(_tmpFilePath.c_str());
            LOGW("Cleaned up interrupted upload temp file: %s", _tmpFilePath.c_str());
        }
    }
}

esp_err_t UploadSession::handleChunk(PsychicRequest* request,
                                     const char* filename,
                                     uint64_t index,
                                     uint8_t* data,
                                     size_t len,
                                     bool final) {
  _lastActivityMs = millis(); // Update session watchdog

  if (!_fs) {
    LOGE("Upload chunk: filesystem not initialized");
    return ESP_FAIL;
  }

  if (index == 0) {
    closeFile();
    _totalBytesWritten = 0;
    _isSuccess = false;
    
    const SYSTEM::PsramString cleanFilename = FileManagerPathUtils::sanitizeFilename(filename);
    const SYSTEM::PsramString outPath = FileManagerPathUtils::joinPaths(_nativeDirectory.c_str(), cleanFilename.c_str());
    SYSTEM::PsramString tmpPath = outPath;
    tmpPath += ".tmp"; // WRITE TO .TMP

    {
      SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
      if (!lock.isLocked()) {
        LOGW("FS mutex timeout during file open for upload");
        return ESP_FAIL;
      }

      if (_fs->exists(outPath.c_str())) {
        LOGW("Upload rejected: file already exists at %s", outPath.c_str());
        return ESP_ERR_INVALID_STATE;
      }

      _currentFile = _fs->open(tmpPath.c_str(), FILE_WRITE);
    }

    if (!_currentFile) {
      LOGE("Failed to create temp file: %s", tmpPath.c_str());
      return ESP_FAIL;
    }
    
    _tmpFilePath = tmpPath;
    _finalFilePath = outPath;
  }

  if (!_currentFile) {
    LOGW("Upload chunk: file descriptor lost");
    return ESP_FAIL;
  }

  // Check upload size limit before writing
  if (_totalBytesWritten + len > _maxFileSize) {
    LOGW("Upload limit exceeded: %zu > %zu", _totalBytesWritten + len, _maxFileSize);
    return ESP_FAIL; // UploadHandler will delete session and trigger destructor cleanup
  }

  size_t written = 0;
  {
    SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
      LOGW("FS mutex timeout during chunk write");
      return ESP_FAIL;
    }
    written = _currentFile.write(data, len);
  }

  if (written != len) {
    LOGE("Failed to write full chunk. Written %zu of %zu", written, len);
    return ESP_FAIL; // Triggers session deletion in UploadHandler
  }
  
  _totalBytesWritten += written;

  if (final) {
    {
      SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
      if (!lock.isLocked()) {
        LOGW("FS mutex timeout during final upload flush");
        return ESP_FAIL;
      }
      _currentFile.flush();
    }

    if (!closeFile()) {
      return ESP_FAIL;
    }

    // ATOMIC RENAME: Replace .tmp with real file only on full success
    if (!_tmpFilePath.empty() && !_finalFilePath.empty() && _fs) {
        const char* tmpPath = _tmpFilePath.c_str();
        const char* finalPath = _finalFilePath.c_str();
        SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (!lock.isLocked()) {
            LOGW("FS mutex timeout during upload finalize");
            return ESP_FAIL;
        }

        if (_fs->rename(tmpPath, finalPath)) {
            _isSuccess = true; // Success! Destructor won't delete the file.
            UploadHooks::handleFileUploaded(_fs, finalPath, _totalBytesWritten);
            LOGI("File successfully finalized: %s", finalPath);
        } else {
            LOGE("Failed to rename %s to %s", tmpPath, finalPath);
            return ESP_FAIL;
        }
    }
  }

  return ESP_OK;
}

bool UploadSession::closeFile() {
  if (_currentFile) {
    SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (lock.isLocked()) {
      _currentFile.close();
      _currentFile = File();
    } else {
      LOGW("closeFile: FS mutex timeout");
      return false;
    }
  }
  return true;
}
