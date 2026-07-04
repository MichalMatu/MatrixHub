#pragma once

#include <PsychicHttpServer.h>
#include <freertos/semphr.h>
#include <memory>

namespace SYSTEM { class HeapMonitor; }

namespace API {
namespace Handlers {

// Forward-declared PIMPL-style cache holder. The handler keeps the cache local to
// the /api/logs list endpoint instead of pushing a pseudo-global cache into the
// filesystem layer.
struct LogFileHandlerListCache;

/**
 * @brief Handler for file system related log endpoints (list, download, delete)
 */
class LogFileHandler {
public:
    LogFileHandler(SemaphoreHandle_t fsMutex, SYSTEM::HeapMonitor* heapMonitor);
    ~LogFileHandler();

    void setFsMutex(SemaphoreHandle_t fsMutex) { _fsMutex = fsMutex; }

    esp_err_t handleList(PsychicRequest* request);
    esp_err_t handleDownload(PsychicRequest* request);
    esp_err_t handleDelete(PsychicRequest* request);

private:
    void invalidateListCache();

    SemaphoreHandle_t _fsMutex;
    SYSTEM::HeapMonitor* _heapMonitor;
    // Persistent per-service cache for the metadata snapshot returned by
    // GET /api/logs. Download/tail paths do not use it.
    std::unique_ptr<LogFileHandlerListCache> _listCache;
};

} // namespace Handlers
} // namespace API
