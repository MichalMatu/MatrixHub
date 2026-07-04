#include "api/filemanager/FileManagerApiService.h"
#include "config/App.h"

#include "system/logging/Logging.h"
#undef LOG_TAG
#define LOG_TAG "FileMgrApi"

namespace API {

FileManagerApiService::FileManagerApiService(PsychicHttpServer* server,
                                             SecurityManager* security,
                                             POWER::PowerManager* powerManager,
                                             FILEMGR::StorageService* storage,
                                             SemaphoreHandle_t fsMutex)
  : BaseApiService(server, security, powerManager, "api/filemanager"),
    _storage(storage),
    _backendResolver(storage),
    _fsMutex(fsMutex),
    _listHandler(&_backendResolver, fsMutex),
    _downloadHandler(&_backendResolver, fsMutex),
    _uploadHandlerBase(&_backendResolver, storage, fsMutex),
    _removeHandler(storage, fsMutex),
    _uploadHandler() {
}

FileManagerApiService::~FileManagerApiService() {
    // Session cleanup is now handled by ~UploadHandler inside _uploadHandlerBase
}

void FileManagerApiService::begin() {
  // Increase upload limit beyond default 512KB for S3
  _server->maxUploadSize = CONFIG::Keys::FILEMGR::kUploadMaxFileSize;

  // GET /rest/fs/list
  _server->on(CONFIG::Keys::kFmListPath, HTTP_GET, wrapAdmin([this](PsychicRequest* request) {
    return _listHandler.handle(request);
  }));

  // GET /rest/fs/download
  _server->on(CONFIG::Keys::kFmDownloadPath, HTTP_GET, wrapAdmin([this](PsychicRequest* request) {
    return _downloadHandler.handle(request);
  }));

  // DELETE /rest/fs/remove
  _server->on(CONFIG::Keys::kFmRemovePath, HTTP_DELETE, wrapAdmin([this](PsychicRequest* request) {
    return _removeHandler.handle(request);
  }));

  // POST /rest/fs/upload
  // Configure the nested _uploadHandler
  _uploadHandler.onRequest([this](PsychicRequest* request) {
    return _uploadHandlerBase.handleComplete(request);
  });
  _uploadHandler.onUpload(
    [this](PsychicRequest* request, const String& filename, uint64_t index, uint8_t* data, size_t len, bool final) {
      return _uploadHandlerBase.handleChunk(request, filename.c_str(), index, data, len, final);
    });
  _uploadHandler.onClose([this](PsychicClient* client) {
    if (!client) {
      return;
    }

    // Production hardening for upload lifecycle: PsychicWebHandler already
    // reports client disconnects, so reuse that existing hook instead of
    // adding another background janitor. This shortens the lifetime of aborted
    // upload sessions and their .tmp files without changing the happy path.
    _uploadHandlerBase.handleClientClose(client->socket());
  });

  // Protect upload route using exact same filter hook
  _uploadHandler.setFilter(_securityManager->filterRequest(AuthenticationPredicates::IS_ADMIN));

  _server->on(CONFIG::Keys::kFmUploadPath, HTTP_POST, &_uploadHandler);
}

}  // namespace API
