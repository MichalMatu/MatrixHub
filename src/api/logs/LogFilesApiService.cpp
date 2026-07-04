#include "LogFilesApiService.h"
#include "../../system/power/PowerManager.h"
#include "../../system/health/heap/HeapMonitor.h"
#include "handlers/LogFileHandler.h"

namespace API {

LogFilesApiService::LogFilesApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, SYSTEM::HeapMonitor* heapMonitor)
    : BaseApiService(server, securityManager, powerManager, "api/logs"),
      _fsMutex(nullptr),
      _heapMonitor(heapMonitor),
      // The handler now owns a short-lived metadata cache for GET /api/logs, so
      // construct it once with the service instead of per request.
      _handler(nullptr, heapMonitor) {}

void LogFilesApiService::begin() {
    _server->on("/api/logs", HTTP_GET, 
                wrapAuth([this](PsychicRequest* request) { 
                    return _handler.handleList(request); 
                }));

    _server->on("/api/logs/download", HTTP_GET, 
                wrapAuth([this](PsychicRequest* request) { 
                    return _handler.handleDownload(request); 
                }));

    _server->on("/api/logs/delete", HTTP_DELETE, 
                wrapAdmin([this](PsychicRequest* request) { 
                    return _handler.handleDelete(request); 
                }));
}

}  // namespace API
