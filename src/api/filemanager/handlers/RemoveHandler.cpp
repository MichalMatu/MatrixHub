#include "api/filemanager/handlers/RemoveHandler.h"
#include "api/BaseApiService.h" // for FS_MUTEX_TIMEOUT_MS
#include "config/App.h"
#include "system/errors/ErrorCodes.h"
#include "system/utils/json/JsonResponseWriter.h"
#include "system/utils/http/HttpError.h"
#include "system/datalogger/BinaryLoggerHelpers.h"
#include "filemanager/infrastructure/support/FileManagerPathUtils.h"
#include "api/filemanager/handlers/FileManagerLogProtection.h"

#include "system/logging/Logging.h"
#undef LOG_TAG
#define LOG_TAG "FmRemove"

namespace API {

RemoveHandler::RemoveHandler(FILEMGR::StorageService* storage, SemaphoreHandle_t fsMutex)
  : _storage(storage), _fsMutex(fsMutex) {}

esp_err_t RemoveHandler::handle(PsychicRequest* request) {
  PsychicWebParameter* param = request->getParam(CONFIG::Keys::kFmParamPath);
  const String requestedPath = param ? param->value() : "";
  if (!requestedPath.length()) {
      return HttpError::send(request, 400, ErrorCodes::Fs::PATH_MISSING, "Missing path parameter");
  }

  SYSTEM::PsramString canonicalRequestedPath;
  if (!FILEMGR::FileManagerPathUtils::canonicalizeAbsolutePath(
          requestedPath.c_str(), "/", canonicalRequestedPath)) {
    LOGW("Remove rejected: invalid path=%s", requestedPath.c_str());
    return HttpError::send(request, 404, ErrorCodes::Fs::INVALID_PATH);
  }

  FS* filesystem = _storage->resolveFilesystem(canonicalRequestedPath);
  const SYSTEM::PsramString nativePath = _storage->toFilesystemPath(canonicalRequestedPath);
  if (!filesystem || nativePath.empty()) {
    LOGW("Remove failed: no filesystem resolved for path=%s", canonicalRequestedPath.c_str());
    return HttpError::send(request, 404, ErrorCodes::Fs::INVALID_PATH);
  }

  if (!FILEMGR::FileManagerPathUtils::isAccessAllowed(
          nativePath, FILEMGR::FileManagerPathUtils::FileManagerPathAccess::Remove)) {
    LOGW("Remove rejected by path policy: request=%s native=%s",
         canonicalRequestedPath.c_str(), nativePath.c_str());
    return HttpError::send(request, 403, ErrorCodes::Fs::PATH_FORBIDDEN);
  }

  char activeLogPath[DATALOG::PATH_BUFFER_SIZE] = {0};
  DATALOG::BinaryLoggerHelpers::getFilePath(activeLogPath, sizeof(activeLogPath));
  if (FILEMANAGER::shouldProtectActiveLogPath(nativePath.c_str(), activeLogPath)) {
    // Logs API already blocks deleting today's active .bin file directly. Mirror
    // that protection in FileManager so admin users cannot bypass it by removing
    // the file itself or one of its parent directories (for example /data or
    // the current month folder) through the generic FS endpoint.
    LOGW("Remove rejected for active log path: request=%s native=%s active=%s",
         canonicalRequestedPath.c_str(), nativePath.c_str(), activeLogPath);
    return HttpError::send(request, 409, ErrorCodes::Logs::ACTIVE_FILE);
  }

  LOGI("Remove path=%s native=%s", canonicalRequestedPath.c_str(), nativePath.c_str());

  // Delegate business logic to StorageService
  bool ok = _storage->removePath(canonicalRequestedPath, _fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));

  if (ok) {
    LOGI("Removed: %s", canonicalRequestedPath.c_str());
    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) return ESP_ERR_HTTPD_RESP_SEND;
    w.raw("{");
    w.key(CONFIG::Keys::kFmOk); w.value(true);
    w.raw("}");
    return w.finish() ? ESP_OK : ESP_FAIL;
  } else {
    LOGW("Remove failed: %s", canonicalRequestedPath.c_str());
    return HttpError::send(request, 500, ErrorCodes::Fs::DELETE_FAILED);
  }
}

}  // namespace API
