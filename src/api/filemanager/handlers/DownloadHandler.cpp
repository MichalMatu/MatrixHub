#include "api/filemanager/handlers/DownloadHandler.h"
#include "api/BaseApiService.h" // for FS_MUTEX_TIMEOUT_MS
#include "config/App.h"
#include "config/System.h"
#include "system/errors/ErrorCodes.h"
#include "system/utils/ScopeLock.h"
#include "system/utils/http/HttpError.h"
#include "system/utils/http/MimeTypes.h"
#include "system/datalogger/BinaryLoggerHelpers.h"
#include "api/filemanager/handlers/FileManagerLogProtection.h"
#include "filemanager/infrastructure/support/FileManagerPathUtils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

#include "system/logging/Logging.h"
#undef LOG_TAG
#define LOG_TAG "FmDownload"

namespace API {

namespace {
constexpr TickType_t kDownloadFsMutexTimeout =
    pdMS_TO_TICKS(API::FILE_DOWNLOAD_FS_MUTEX_TIMEOUT_MS);
}

DownloadHandler::DownloadHandler(FILEMGR::FileManagerBackendResolver* resolver, SemaphoreHandle_t fsMutex)
  : _backendResolver(resolver), _fsMutex(fsMutex) {}

esp_err_t DownloadHandler::handle(PsychicRequest* request) {
  PsychicWebParameter* param = request->getParam(CONFIG::Keys::kFmParamPath);
  static const String kEmptyPath("");
  const String& requestedPath = param ? param->value() : kEmptyPath;
  if (!requestedPath.length()) return HttpError::send(request, 400, ErrorCodes::Fs::PATH_MISSING, "Missing path parameter");

  SYSTEM::PsramString canonicalRequestedPath;
  if (!FILEMGR::FileManagerPathUtils::canonicalizeAbsolutePath(
          requestedPath.c_str(), "/", canonicalRequestedPath)) {
    LOGW("Download rejected: invalid path=%s", requestedPath.c_str());
    return HttpError::send(request, 404, ErrorCodes::Fs::INVALID_PATH);
  }

  LOGI("Download path=%s", canonicalRequestedPath.c_str());

  File file;
  SYSTEM::PsramString nativePath;
  size_t fileSize = 0;

  {
    SYSTEM::ScopeLock lock(_fsMutex, kDownloadFsMutexTimeout);
    if (!lock.isLocked()) {
      LOGW("FS mutex timeout on download");
      return HttpError::send(request, 503, ErrorCodes::Busy::FILESYSTEM_BUSY, "Filesystem busy");
    }

    FILEMGR::FileManagerBackendResolution resolution = _backendResolver->resolveForPath(canonicalRequestedPath);
    if (!resolution.filesystem) return HttpError::send(request, 404, ErrorCodes::Fs::INVALID_PATH);

    if (!FILEMGR::FileManagerPathUtils::isAccessAllowed(
            resolution.nativePath.c_str(), FILEMGR::FileManagerPathUtils::FileManagerPathAccess::Download)) {
      LOGW("Download rejected by path policy: request=%s native=%s",
           canonicalRequestedPath.c_str(), resolution.nativePath.c_str());
      return HttpError::send(request, 403, ErrorCodes::Fs::PATH_FORBIDDEN);
    }

    nativePath = resolution.nativePath;
    if (!resolution.filesystem->exists(nativePath.c_str())) {
      LOGW("File not found: %s", nativePath.c_str());
      return HttpError::send(request, 404, ErrorCodes::Fs::FILE_NOT_FOUND);
    }

    file = resolution.filesystem->open(nativePath.c_str(), "r");
    if (file) fileSize = file.size();
  } // Mutex released here - safety provided by point-locking below

  if (!file) {
    LOGW("Failed to open file: %s", nativePath.c_str());
    return HttpError::send(request, 500, ErrorCodes::Fs::OPEN_FAILED);
  }

  const bool needsLogReadLease = FILEMANAGER::shouldAcquireLogReadLease(
      nativePath.c_str(), fileSize, CONFIG::FILEMGR::kInlineMaxBytes);
  bool leaseAcquired = false;
  if (needsLogReadLease) {
    // Logs API already protects streamed /data/*.bin downloads with a read
    // lease so delete/rotation will not race with an in-flight transfer. Mirror
    // that behavior here to keep FileManager downloads on the same safety path
    // without changing access policy or the user-facing flow.
    leaseAcquired = DATALOG::BinaryLoggerHelpers::acquireReadLease(nativePath.c_str());
    if (!leaseAcquired) {
      file.close();
      LOGW("Download rejected: log file busy for lease acquisition: %s", nativePath.c_str());
      return HttpError::send(request, 503, ErrorCodes::Busy::FILESYSTEM_BUSY, "Filesystem busy");
    }
  }

  httpd_req_t* rawReq = request->request();
  httpd_resp_set_status(rawReq, "200 OK");
  
  // Try to determine Content-Type or fallback
  const char* contentType = API::MimeTypes::getContentType(nativePath.c_str());
  httpd_resp_set_type(rawReq, contentType);
  
  // Force download (matching 'true' in PsychicFileResponse)
  const char* filenamePtr = strrchr(nativePath.c_str(), '/');
  filenamePtr = filenamePtr ? (filenamePtr + 1) : nativePath.c_str();
  SYSTEM::PsramString disposition;
  disposition.reserve(strlen(filenamePtr) + 32);
  disposition = "attachment; filename=\"";
  disposition += filenamePtr;
  disposition += "\"";
  httpd_resp_set_hdr(rawReq, "Content-Disposition", disposition.c_str());

  // Optimal allocation in PSRAM
  size_t bufSize = (fileSize <= CONFIG::FILEMGR::kInlineMaxBytes)
                     ? CONFIG::FILEMGR::kInlineMaxBytes
                     : CONFIG::FILEMGR::kChunkSizeBytes;
  uint8_t* buffer = (uint8_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);

  if (!buffer) {
     file.close();
     if (leaseAcquired) {
       DATALOG::BinaryLoggerHelpers::releaseReadLease(nativePath.c_str());
     }
     return HttpError::send(request, 500, ErrorCodes::Internal::OUT_OF_MEMORY);
  }

  esp_err_t err = ESP_OK;

  if (fileSize <= CONFIG::FILEMGR::kInlineMaxBytes) {
      // Fast read for small files
      SYSTEM::ScopeLock lock(_fsMutex, kDownloadFsMutexTimeout);
      if (lock.isLocked()) {
          size_t readLen = file.read(buffer, fileSize);
          lock.unlock();
          err = httpd_resp_send(rawReq, reinterpret_cast<const char*>(buffer), (ssize_t)readLen);
      } else {
          err = ESP_FAIL;
      }
  } else {
      // Chunked streaming (Point-Locking) protecting Watchdog
      while (true) {
          SYSTEM::ScopeLock readLock(_fsMutex, kDownloadFsMutexTimeout);
          if (!readLock.isLocked()) {
              LOGW("FS busy during FileMgr chunked read");
              err = ESP_FAIL;
              break;
          }
          size_t readLen = file.read(buffer, CONFIG::FILEMGR::kChunkSizeBytes);
          readLock.unlock();

          if (readLen == 0) break; // EOF
          
          err = httpd_resp_send_chunk(rawReq, reinterpret_cast<const char*>(buffer), (ssize_t)readLen);
          if (err != ESP_OK) break; // Network error

          vTaskDelay(pdMS_TO_TICKS(CONFIG::FILEMGR::kDownloadYieldMs));
      }
      if (err == ESP_OK) {
          err = httpd_resp_send_chunk(rawReq, nullptr, 0); // End HTTP Chunked transfer
      }
  }
  
  file.close();
  if (leaseAcquired) {
    DATALOG::BinaryLoggerHelpers::releaseReadLease(nativePath.c_str());
  }
  heap_caps_free(buffer);
  
  if (err != ESP_OK && !httpd_req_to_sockfd(rawReq)) {
      // If error occurs and it's not a generic network termination, we can't easily resend JSON at this point,
      // because headers might have already been sent. We just return the error.
  }
  
  return err;
}

}  // namespace API
