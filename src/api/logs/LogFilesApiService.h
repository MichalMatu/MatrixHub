#pragma once

/**
 * @file LogFilesApiService.h
 * @brief Routing facade for sensor log file endpoints under /api/logs/*
 */

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "handlers/LogFileHandler.h"
#include "freertos/semphr.h"

namespace SYSTEM { class HeapMonitor; }

namespace API {

/**
 * @brief Routing facade for historical sensor log file management endpoints
 *
 * Registers:
 * - GET /api/logs
 * - GET /api/logs/download
 * - DELETE /api/logs/delete
 */
class LogFilesApiService : public BaseApiService {
public:
    LogFilesApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager, SYSTEM::HeapMonitor* heapMonitor);
    
    void setFsMutex(SemaphoreHandle_t fsMutex) {
        _fsMutex = fsMutex;
        // The handler is long-lived now so it can retain the short-lived logs
        // list cache across requests. Keep its FS mutex in sync with the service
        // wiring rather than constructing a fresh handler every time.
        _handler.setFsMutex(fsMutex);
    }
    
    /**
     * @brief Register historical log file endpoints with authentication
     */
    void begin() override;
    
private:
    SemaphoreHandle_t _fsMutex = nullptr;
    SYSTEM::HeapMonitor* _heapMonitor;
    // Keep one handler instance for the lifetime of this API service so request-
    // local code can reuse the metadata cache for GET /api/logs.
    Handlers::LogFileHandler _handler;
};

}  // namespace API
