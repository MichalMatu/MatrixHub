#pragma once

#include "filemanager/infrastructure/uploads/UploadSession.h"
#include <map>
#include <memory>
#include <mutex>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "filemanager/infrastructure/backends/resolver/FileManagerBackendResolver.h"
#include "filemanager/infrastructure/backends/service/StorageService.h"

namespace API {

// Custom deleter that safely destroys the object and frees PSRAM memory
struct PsramDeleter {
    void operator()(FILEMGR::UploadSession* ptr) const {
        if (ptr) {
            ptr->~UploadSession();
            heap_caps_free(ptr);
        }
    }
};

using UploadSessionPtr = std::unique_ptr<FILEMGR::UploadSession, PsramDeleter>;

class UploadHandler {
 public:
  UploadHandler(FILEMGR::FileManagerBackendResolver* resolver, 
                FILEMGR::StorageService* storage, 
                SemaphoreHandle_t fsMutex);
  ~UploadHandler();

  esp_err_t handleChunk(PsychicRequest* request, const char* filename,
                        uint64_t index, uint8_t* data, size_t len, bool final);
  esp_err_t handleComplete(PsychicRequest* request);
  // Fast-path cleanup for abandoned uploads. The periodic stale-session sweep
  // is kept as a fallback, but disconnect should usually reclaim .tmp files
  // immediately instead of waiting for another upload request.
  void handleClientClose(int clientId);

 private:
  void cleanupStaleSessions();

  FILEMGR::FileManagerBackendResolver* _backendResolver;
  FILEMGR::StorageService* _storage;
  SemaphoreHandle_t _fsMutex;

  std::map<int, UploadSessionPtr> _uploadSessions;
  SemaphoreHandle_t _sessionsMutex;
};

}  // namespace API
