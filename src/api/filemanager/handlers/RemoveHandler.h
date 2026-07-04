#pragma once

#include <PsychicHttp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "filemanager/infrastructure/backends/service/StorageService.h"

namespace API {

class RemoveHandler {
 public:
  RemoveHandler(FILEMGR::StorageService* storage, SemaphoreHandle_t fsMutex);
  esp_err_t handle(PsychicRequest* request);

 private:
  FILEMGR::StorageService* _storage;
  SemaphoreHandle_t _fsMutex;
};

}  // namespace API
