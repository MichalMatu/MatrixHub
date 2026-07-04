#pragma once

#include <FS.h>
#include <PsychicHttp.h>

#include <Arduino.h>
#include "system/memory/PsramAllocator.h"

namespace FILEMGR {

class UploadSession {
 public:
  static constexpr size_t kDefaultMaxFileSize = 10 * 1024 * 1024;  // 10 MB default sanity limit

  UploadSession(FS* fs, const char* nativeDirectory, SemaphoreHandle_t fsMutex, size_t maxFileSize = kDefaultMaxFileSize);
  ~UploadSession();

  esp_err_t handleChunk(PsychicRequest* request, const char* filename,
                        uint64_t index, uint8_t* data, size_t len, bool final);
  
  uint32_t getLastActivity() const { return _lastActivityMs; }
  bool isSuccess() const { return _isSuccess; }

 private:
  bool closeFile();

  FS* _fs;
  SYSTEM::PsramString _nativeDirectory;
  SemaphoreHandle_t _fsMutex;
  File _currentFile;
  
  SYSTEM::PsramString _tmpFilePath;   // Working path (.tmp)
  SYSTEM::PsramString _finalFilePath; // Target path

  size_t _totalBytesWritten;
  size_t _maxFileSize;
  uint32_t _lastActivityMs;
  bool _isSuccess;
};

}  // namespace FILEMGR
