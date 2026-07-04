#pragma once

#include <PsychicHttp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "filemanager/infrastructure/backends/resolver/FileManagerBackendResolver.h"

namespace API {

class ListHandler {
 public:
  ListHandler(FILEMGR::FileManagerBackendResolver* resolver, SemaphoreHandle_t fsMutex);
  esp_err_t handle(PsychicRequest* request);

 private:
  FILEMGR::FileManagerBackendResolver* _backendResolver;
  SemaphoreHandle_t _fsMutex;
};

}  // namespace API
