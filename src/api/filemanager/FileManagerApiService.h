#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <esp_err.h>
#include <PsychicHttp.h>

#include "security/SecurityManager.h"
#include "api/BaseApiService.h"

#include "filemanager/infrastructure/backends/resolver/FileManagerBackendResolver.h"

// Handlers
#include "api/filemanager/handlers/ListHandler.h"
#include "api/filemanager/handlers/DownloadHandler.h"
#include "api/filemanager/handlers/UploadHandler.h"
#include "api/filemanager/handlers/RemoveHandler.h"

namespace FILEMGR {
class StorageService;
}

namespace API {

class FileManagerApiService : public BaseApiService {
 public:
  FileManagerApiService(PsychicHttpServer* server,
                        SecurityManager* security,
                        POWER::PowerManager* powerManager,
                        FILEMGR::StorageService* storage,
                        SemaphoreHandle_t fsMutex);

  // Cleanly managed via RAII/Destructor if active
  ~FileManagerApiService();

  void begin() override;

 private:
  // Private Dependencies
  FILEMGR::StorageService* _storage;
  FILEMGR::FileManagerBackendResolver _backendResolver;
  SemaphoreHandle_t _fsMutex;

  // Handlers
  ListHandler _listHandler;
  DownloadHandler _downloadHandler;
  UploadHandler _uploadHandlerBase;
  RemoveHandler _removeHandler;

  // State Management
  PsychicUploadHandler _uploadHandler;
};

}  // namespace API
