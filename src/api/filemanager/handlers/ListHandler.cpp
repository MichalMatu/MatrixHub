#include "api/filemanager/handlers/ListHandler.h"
#include "config/App.h"
#include "system/errors/ErrorCodes.h"
#include "system/utils/json/JsonResponseWriter.h"
#include "system/utils/http/HttpError.h"
#include "filemanager/infrastructure/support/FileManagerPathUtils.h"
#include "filemanager/infrastructure/listing/DirectoryLister.h"

#include "system/logging/Logging.h"
#undef LOG_TAG
#define LOG_TAG "FmList"

namespace API {

ListHandler::ListHandler(FILEMGR::FileManagerBackendResolver* resolver, SemaphoreHandle_t fsMutex)
  : _backendResolver(resolver), _fsMutex(fsMutex) {}

esp_err_t ListHandler::handle(PsychicRequest* request) {
  PsychicWebParameter* param = request->getParam(CONFIG::Keys::kFmParamDir);
  static const String kRootPath("/");
  const String& dirParam = param ? param->value() : kRootPath;
  const String& requestedPath = dirParam.length() ? dirParam : kRootPath;
  SYSTEM::PsramString canonicalRequestedPath;

  if (!FILEMGR::FileManagerPathUtils::canonicalizeAbsoluteDirectory(
          requestedPath.c_str(), "/", canonicalRequestedPath)) {
    LOGW("List rejected: invalid dir=%s", requestedPath.c_str());
    return HttpError::send(request, 404, ErrorCodes::Fs::INVALID_PATH);
  }

  LOGI("List dir=%s", canonicalRequestedPath.c_str());

  FILEMGR::FileManagerBackendResolution resolution = _backendResolver->resolveForPath(canonicalRequestedPath);

  if (!resolution.filesystem) {
    LOGW("No filesystem for path: %s", canonicalRequestedPath.c_str());
    return HttpError::send(request, 404, ErrorCodes::Fs::INVALID_PATH);
  }

  if (!FILEMGR::FileManagerPathUtils::isAccessAllowed(
          resolution.nativePath.c_str(), FILEMGR::FileManagerPathUtils::FileManagerPathAccess::List)) {
    LOGW("List rejected by path policy: request=%s native=%s",
         canonicalRequestedPath.c_str(), resolution.nativePath.c_str());
    return HttpError::send(request, 403, ErrorCodes::Fs::PATH_FORBIDDEN);
  }

  FILEMGR::DirectoryLister lister(
      resolution.filesystem, canonicalRequestedPath.c_str(), resolution.nativePath.c_str(), _fsMutex);
  const FILEMGR::DirectoryLister::PopulateStatus status = lister.prepare();

  if (status != FILEMGR::DirectoryLister::PopulateStatus::Success) {
    switch (status) {
      case FILEMGR::DirectoryLister::PopulateStatus::FilesystemBusy:
        LOGW("List failed: filesystem busy for path=%s", canonicalRequestedPath.c_str());
        return HttpError::send(request, 503, ErrorCodes::Busy::FILESYSTEM_BUSY, "Filesystem busy");
      case FILEMGR::DirectoryLister::PopulateStatus::FileNotFound:
        LOGW("List failed: path not found=%s", canonicalRequestedPath.c_str());
        return HttpError::send(request, 404, ErrorCodes::Fs::FILE_NOT_FOUND);
      case FILEMGR::DirectoryLister::PopulateStatus::OpenFailed:
        LOGW("List failed: open failed for path=%s", canonicalRequestedPath.c_str());
        return HttpError::send(request, 500, ErrorCodes::Fs::OPEN_FAILED);
      case FILEMGR::DirectoryLister::PopulateStatus::Success:
        break;
    }
  }

  Utils::JsonResponseWriter w(request->request());
  if (!w.beginResponse()) {
    return ESP_ERR_HTTPD_RESP_SEND;
  }

  w.raw("{");
  w.key(CONFIG::Keys::kFmFiles);
  w.raw("[");

  lister.emitPrepared(w);

  w.raw("]}");
  return w.finish() ? ESP_OK : ESP_FAIL;
}

}  // namespace API
