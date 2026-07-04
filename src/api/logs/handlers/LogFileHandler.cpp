#include "LogFileHandler.h"
#include "../../../config/System.h"
#include "../../../config/App.h"
#include "../../../system/datalogger/BinaryLoggerHelpers.h"
#include "../../../system/memory/PsramAllocator.h"
#include "../../../system/utils/ScopeLock.h"
#include "../../../system/health/heap/HeapMonitor.h"
#include "../../../system/errors/ErrorCodes.h"
#include "../../../system/utils/json/JsonResponseWriter.h"
#include "../../../system/logging/Logging.h"
#include <utils/ResponseUtils.h>
#include <LittleFS.h>
#include <esp_http_server.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <array>
#include <utility>

#undef LOG_TAG
#define LOG_TAG "LogFile"

namespace API {
namespace Handlers {

struct LogFileEntry {
    SYSTEM::PsramString name;
    uint32_t size = 0;
};

using LogFileEntries = SYSTEM::PsramVector<LogFileEntry>;

struct LogMonthEntry {
    SYSTEM::PsramString path;
    SYSTEM::PsramString name;
    LogFileEntries files;
};

using LogMonthEntries = SYSTEM::PsramVector<LogMonthEntry>;

struct LogFileHandlerListCache {
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    LogMonthEntries months;
    uint32_t snapshotAtMs = 0;
    bool valid = false;
};

namespace {

enum class SnapshotStatus : uint8_t {
    Success,
    Busy,
    OpenFailed,
};
// The log list endpoint only needs filesystem metadata (names + sizes), not the
// file bodies. We therefore snapshot the directory tree into PSRAM first and
// stream JSON afterwards. This keeps the HTTP response deterministic and avoids
// interleaving LittleFS traversal with chunked response writes.
using LogFileBatch = std::array<LogFileEntry, LOG_CFG::LOG_FILE_LIST_SCAN_BATCH_ENTRIES>;
using LogMonthBatch = std::array<LogMonthEntry, LOG_CFG::LOG_FILE_LIST_SCAN_BATCH_ENTRIES>;

const char* baseName(const char* path) {
    if (!path) {
        return "";
    }

    const char* slash = strrchr(path, '/');
    return (slash && *(slash + 1) != '\0') ? (slash + 1) : path;
}

SnapshotStatus closeFileLocked(File& file, SemaphoreHandle_t fsMutex) {
    if (!file) {
        return SnapshotStatus::Success;
    }

    SYSTEM::ScopeLock lock(fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        return SnapshotStatus::Busy;
    }

    file.close();
    return SnapshotStatus::Success;
}

void yieldLogListScan() {
    if (LOG_CFG::LOG_FILE_LIST_YIELD_MS == 0) {
        return;
    }

    // Yield only once per batch instead of once per file. The older per-entry
    // delay made /api/logs latency grow linearly with directory depth and entry
    // count even when we only had a handful of files.
    vTaskDelay(pdMS_TO_TICKS(LOG_CFG::LOG_FILE_LIST_YIELD_MS));
}

void yieldChunkedDownloadProgress(size_t chunksSinceYield) {
    // Chunked downloads already release the FS mutex around every 1 KB read, so
    // we only need an occasional scheduler yield here. The old per-chunk 1 ms
    // delay scaled linearly with file size and made larger log downloads
    // noticeably slower in the admin UI.
    static constexpr size_t kChunksPerYield = 16;
    if (chunksSinceYield >= kChunksPerYield) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

SnapshotStatus snapshotMonthFileBatch(File& monthDir,
                                      SemaphoreHandle_t fsMutex,
                                      LogFileBatch& batch,
                                      size_t& batchCount,
                                      bool& exhausted) {
    batchCount = 0;
    exhausted = false;

    SYSTEM::ScopeLock lock(fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        return SnapshotStatus::Busy;
    }

    // Keep the FS lock for one compact batch so openNextFile()/size()/close()
    // do not thrash the mutex on every single file. We still cap the batch to
    // preserve fairness for other LittleFS users.
    while (batchCount < batch.size()) {
        File file = monthDir.openNextFile();
        if (!file) {
            exhausted = true;
            break;
        }

        if (!file.isDirectory()) {
            LogFileEntry entry;
            entry.name = SYSTEM::makePsramString(baseName(file.name()));
            entry.size = static_cast<uint32_t>(file.size());
            batch[batchCount++] = std::move(entry);
        }

        file.close();
    }

    return SnapshotStatus::Success;
}

SnapshotStatus snapshotMonthFiles(File& monthDir, SemaphoreHandle_t fsMutex, LogFileEntries& files) {
    LogFileBatch batch{};
    bool exhausted = false;

    while (!exhausted) {
        size_t batchCount = 0;
        const SnapshotStatus status =
            snapshotMonthFileBatch(monthDir, fsMutex, batch, batchCount, exhausted);
        if (status != SnapshotStatus::Success) {
            return status;
        }

        for (size_t i = 0; i < batchCount; ++i) {
            files.push_back(std::move(batch[i]));
        }

        if (!exhausted && batchCount > 0) {
            yieldLogListScan();
        }
    }

    return SnapshotStatus::Success;
}

SnapshotStatus snapshotSingleMonth(const SYSTEM::PsramString& monthPath,
                                   SemaphoreHandle_t fsMutex,
                                   LogFileEntries& files) {
    File monthDir;

    {
        SYSTEM::ScopeLock lock(fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (!lock.isLocked()) {
            return SnapshotStatus::Busy;
        }

        monthDir = LittleFS.open(monthPath.c_str(), "r");
        if (!monthDir || !monthDir.isDirectory()) {
            if (monthDir) {
                monthDir.close();
            }
            return SnapshotStatus::OpenFailed;
        }
    }

    const SnapshotStatus status = snapshotMonthFiles(monthDir, fsMutex, files);
    const SnapshotStatus closeStatus = closeFileLocked(monthDir, fsMutex);

    if (status != SnapshotStatus::Success) {
        return status;
    }

    return closeStatus;
}

SnapshotStatus snapshotLogMonths(SemaphoreHandle_t fsMutex, LogMonthEntries& months) {
    File dataRoot;

    {
        SYSTEM::ScopeLock lock(fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (!lock.isLocked()) {
            return SnapshotStatus::Busy;
        }

        dataRoot = LittleFS.open("/data", "r");
        if (!dataRoot) {
            return SnapshotStatus::Success;
        }

        if (!dataRoot.isDirectory()) {
            dataRoot.close();
            return SnapshotStatus::Success;
        }
    }

    SnapshotStatus status = SnapshotStatus::Success;

    LogMonthBatch batch{};
    bool exhausted = false;

    while (!exhausted) {
        size_t batchCount = 0;

        {
            SYSTEM::ScopeLock lock(fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            if (!lock.isLocked()) {
                status = SnapshotStatus::Busy;
                break;
            }

            while (batchCount < batch.size()) {
                File dir = dataRoot.openNextFile();
                if (!dir) {
                    exhausted = true;
                    break;
                }

                if (dir.isDirectory()) {
                    LogMonthEntry month;
                    const char* monthPath = dir.path();
                    month.path = SYSTEM::makePsramString(monthPath ? monthPath : "/data");
                    month.name = SYSTEM::makePsramString(baseName(monthPath));
                    batch[batchCount++] = std::move(month);
                }

                dir.close();
            }
        }

        for (size_t i = 0; i < batchCount; ++i) {
            status = snapshotSingleMonth(batch[i].path, fsMutex, batch[i].files);
            if (status != SnapshotStatus::Success) {
                break;
            }

            if (!batch[i].files.empty()) {
                months.push_back(std::move(batch[i]));
            }
        }

        if (status != SnapshotStatus::Success) {
            break;
        }

        if (!exhausted && batchCount > 0) {
            yieldLogListScan();
        }
    }

    const SnapshotStatus closeStatus = closeFileLocked(dataRoot, fsMutex);
    if (status != SnapshotStatus::Success) {
        return status;
    }

    return closeStatus;
}

esp_err_t writeLogMonthsResponse(PsychicRequest* request, const LogMonthEntries& months) {
    Utils::JsonResponseWriter w(request->request());
    if (!w.beginResponse()) {
        return ESP_FAIL;
    }

    w.raw("{\"months\":[");

    for (size_t monthIndex = 0; monthIndex < months.size(); ++monthIndex) {
        if (monthIndex > 0) {
            w.raw(",");
        }

        const LogMonthEntry& month = months[monthIndex];
        w.raw("{");
        w.key("path");
        w.string(month.path.c_str());
        w.raw(",");
        w.key("name");
        w.string(month.name.c_str());
        w.raw(",\"files\":[");

        for (size_t fileIndex = 0; fileIndex < month.files.size(); ++fileIndex) {
            if (fileIndex > 0) {
                w.raw(",");
            }

            const LogFileEntry& file = month.files[fileIndex];
            w.raw("{");
            w.key("name");
            w.string(file.name.c_str());
            w.raw(",");
            w.key("size");
            w.value(file.size);
            w.raw("}");
        }

        w.raw("]}");
    }

    w.raw("]}");

    return w.finish() ? ESP_OK : ESP_FAIL;
}

}  // namespace

LogFileHandler::LogFileHandler(SemaphoreHandle_t fsMutex, SYSTEM::HeapMonitor* heapMonitor)
    : _fsMutex(fsMutex), _heapMonitor(heapMonitor), _listCache(std::make_unique<LogFileHandlerListCache>()) {}

LogFileHandler::~LogFileHandler() {
    if (_listCache && _listCache->mutex) {
        vSemaphoreDelete(_listCache->mutex);
        _listCache->mutex = nullptr;
    }
}

void LogFileHandler::invalidateListCache() {
    if (!_listCache || !_listCache->mutex) {
        return;
    }

    SYSTEM::ScopeLock cacheLock(_listCache->mutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!cacheLock.isLocked()) {
        return;
    }

    _listCache->months.clear();
    _listCache->snapshotAtMs = 0;
    _listCache->valid = false;
}

esp_err_t LogFileHandler::handleList(PsychicRequest* request) {
    if (_listCache && _listCache->mutex) {
        SYSTEM::ScopeLock cacheLock(_listCache->mutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (cacheLock.isLocked() && _listCache->valid) {
            const uint32_t ageMs = (uint32_t)millis() - _listCache->snapshotAtMs;
            if (ageMs <= LOG_CFG::LOG_FILE_LIST_CACHE_TTL_MS) {
                // Cache only the filesystem metadata snapshot. Rendering still
                // goes through the normal response writer, but repeated page
                // opens no longer rescan /data immediately after the previous
                // request. TTL is evaluated lazily here on each request; there
                // is no timer task that proactively expires the cache.
                return writeLogMonthsResponse(request, _listCache->months);
            }
        }
    }

    LogMonthEntries months;

    // Cache misses still build the filesystem snapshot first so fs/busy can fail
    // cleanly instead of returning a partially streamed JSON payload.
    switch (snapshotLogMonths(_fsMutex, months)) {
        case SnapshotStatus::Busy:
            return Response::error(request, 503, "fs/busy");
        case SnapshotStatus::OpenFailed:
            return Response::error(request, 500, "fs/open_failed");
        case SnapshotStatus::Success:
            break;
    }

    if (_listCache && _listCache->mutex) {
        SYSTEM::ScopeLock cacheLock(_listCache->mutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (cacheLock.isLocked()) {
            _listCache->months = std::move(months);
            _listCache->snapshotAtMs = (uint32_t)millis();
            _listCache->valid = true;
            return writeLogMonthsResponse(request, _listCache->months);
        }
    }

    return writeLogMonthsResponse(request, months);
}

esp_err_t LogFileHandler::handleDownload(PsychicRequest* request) {
    uint32_t heapBefore = ESP.getFreeHeap();
    uint32_t largestBefore = ESP.getMaxAllocHeap();

    if (!request->hasParam("file")) {
        LOGW("Download request missing file parameter");
        return Response::error(request, 400, "missing_param");
    }

    const char* filename = request->getParam("file")->value().c_str();
    LOGI("Download request for: %s", filename);

    if (strncmp(filename, "/data/", 6) != 0 || strstr(filename, "..") != nullptr) {
        LOGW("Invalid file path (not under /data or traversal): %s", filename);
        return Response::error(request, 400, "missing_param");
    }

    SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));

    if (!lock.isLocked()) {
        LOGW("Filesystem busy for: %s", filename);
        return Response::error(request, 503, "fs/busy");
    }

    if (!LittleFS.exists(filename)) {
        LOGW("File not found: %s", filename);
        return Response::error(request, 404, "not_found");
    }

    File file = LittleFS.open(filename, "r");
    if (!file) {
        LOGE("Failed to open file: %s", filename);
        return Response::error(request, 500, "fs/open_failed");
    }

    const size_t fileSize = file.size();
    const bool chunkedTransfer = fileSize > 4096;
    bool leaseAcquired = false;

    // Inline downloads still own the broad FS lock at cleanup time, while the
    // chunked path explicitly unlocks early and must reacquire the FS mutex for
    // the final file.close() to stay consistent with the rest of this handler.
    auto closeAndRelease = [&](bool fileAlreadyLocked) {
        if (file) {
            if (fileAlreadyLocked) {
                file.close();
            } else {
                const SnapshotStatus closeStatus = closeFileLocked(file, _fsMutex);
                if (closeStatus == SnapshotStatus::Busy) {
                    // Best-effort fallback: the normal path closes under the FS
                    // mutex, but we still prefer releasing the handle now over
                    // leaking it until scope exit if another FS user holds the
                    // lock at the tail end of a chunked download.
                    LOGW("FS busy during download close, using unlocked fallback");
                    file.close();
                }
            }
        }
        if (leaseAcquired) {
            DATALOG::BinaryLoggerHelpers::releaseReadLease(filename);
            leaseAcquired = false;
        }
    };

    if (chunkedTransfer) {
        leaseAcquired = DATALOG::BinaryLoggerHelpers::acquireReadLease(filename);
        if (!leaseAcquired) {
            closeAndRelease(true);
            return Response::error(request, 503, "fs/busy");
        }
    }

    httpd_req_t* rawReq = request->request();
    esp_err_t err = ESP_OK;

    err = httpd_resp_set_status(rawReq, "200 OK");
    if (err != ESP_OK) {
        closeAndRelease(true);
        return err;
    }

    err = httpd_resp_set_type(rawReq, "application/octet-stream");
    if (err != ESP_OK) {
        closeAndRelease(true);
        return err;
    }

    err = httpd_resp_set_hdr(rawReq, "Content-Disposition", "attachment");
    if (err != ESP_OK) {
        closeAndRelease(true);
        return err;
    }

    static constexpr size_t kInlineMax = 4096;
    size_t bufSize = (fileSize <= kInlineMax) ? kInlineMax : 1024;
    uint8_t* buffer = (uint8_t*)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM);

    if (!buffer) {
       LOGE("Failed to allocate PSRAM buffer (%u bytes)", (unsigned)bufSize);
       closeAndRelease(true);
       return ESP_ERR_NO_MEM;
    }

    if (fileSize <= kInlineMax) {
        LOGI("Sending file inline: %s (size: %u bytes)", filename, (unsigned)fileSize);

        const size_t readLen = file.read(buffer, fileSize);
        if (readLen != fileSize) {
            LOGE("Short read for: %s (expected %u, got %u)", filename, (unsigned)fileSize, (unsigned)readLen);
            err = ESP_FAIL;
        }

        closeAndRelease(true);
        lock.unlock();

        if (err == ESP_OK) {
            err = httpd_resp_send(rawReq, reinterpret_cast<const char*>(buffer), (ssize_t)readLen);
        }
    } else {
        LOGI("Streaming file (chunked): %s (size: %u bytes)", filename, (unsigned)fileSize);

        // Release the broad lock — we'll use point-locking per chunk
        lock.unlock();
        size_t chunksSinceYield = 0;

        while (true) {
            // Point-lock: acquire only for file.read()
            SYSTEM::ScopeLock readLock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            if (!readLock.isLocked()) {
                LOGW("FS busy during chunked read, aborting download");
                err = ESP_FAIL;
                break;
            }
            const size_t readLen = file.read(buffer, 1024);
            readLock.unlock();
            // Lock released — FS is free for SensorLogger and others

            if (readLen == 0) {
                break;
            }
            err = httpd_resp_send_chunk(rawReq, reinterpret_cast<const char*>(buffer), (ssize_t)readLen);
            if (err != ESP_OK) {
                break;
            }
            ++chunksSinceYield;
            yieldChunkedDownloadProgress(chunksSinceYield);
            if (chunksSinceYield >= 16) {
                chunksSinceYield = 0;
            }
        }

        closeAndRelease(false);

        if (err == ESP_OK) {
            err = httpd_resp_send_chunk(rawReq, nullptr, 0);
        }
    }
    
    heap_caps_free(buffer);

    uint32_t heapAfter = ESP.getFreeHeap();
    uint32_t largestAfter = ESP.getMaxAllocHeap();
    int32_t deltaFree = (int32_t)heapAfter - (int32_t)heapBefore;
    int32_t deltaLargest = (int32_t)largestAfter - (int32_t)largestBefore;

    if (abs(deltaFree) > 500 || deltaLargest < -1000) {
        LOGW("[WebUI] /api/logs/download: deltaFree=%+d deltaLargest=%+d (now: %u/%u)",
                deltaFree, deltaLargest, heapAfter, largestAfter);
    }

    if (deltaLargest <= -2048) {
        if (_heapMonitor) {
            _heapMonitor->armDeferredProbe("download", 250, heapAfter, largestAfter);
        }
    }

    if (err == ESP_OK) {
        LOGI("Download complete: %s (%u bytes)", filename, (unsigned)fileSize);
    } else {
        LOGE("Download failed: %s (err=%d)", filename, err);
    }

    return err;
}

esp_err_t LogFileHandler::handleDelete(PsychicRequest* request) {
    if (!request->hasParam("file")) {
        return Response::error(request, 400, "missing_param");
    }

    const char* filename = request->getParam("file")->value().c_str();

    // Path validation: must be under /data/ and must not contain path traversal
    if (strncmp(filename, "/data/", 6) != 0 || strstr(filename, "..") != nullptr) {
        LOGW("Invalid delete path (not under /data or traversal): %s", filename);
        return Response::error(request, 400, "missing_param");
    }

    SYSTEM::ScopeLock lock(_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!lock.isLocked()) {
        return Response::error(request, 503, "fs/busy");
    }

    char activeLogPath[DATALOG::PATH_BUFFER_SIZE] = {0};
    DATALOG::BinaryLoggerHelpers::getFilePath(activeLogPath, sizeof(activeLogPath));
    if (activeLogPath[0] != '\0' && strcmp(filename, activeLogPath) == 0) {
        // Refuse deleting today's active .bin file while the logger may still
        // append to it. Allowing the delete would not crash immediately, but it
        // would split the current day's history into "before delete" and "after
        // recreate" segments and make the data timeline much harder to reason
        // about from the admin UI.
        return Response::error(request, 409, ErrorCodes::Logs::ACTIVE_FILE);
    }

    if (DATALOG::BinaryLoggerHelpers::isReadLeaseActive(filename)) {
        return Response::error(request, 503, "fs/busy");
    }

    if (!LittleFS.exists(filename)) {
        return Response::error(request, 404, "not_found");
    }

    bool deleted = LittleFS.remove(filename);

    if (!deleted) {
        return Response::error(request, 500, "internal/delete_failed");
    }

    // Delete is the one log-list mutation we perform directly in this handler,
    // so invalidate immediately instead of waiting for the passive TTL check.
    invalidateListCache();

    return Response::success(request, [](JsonVariant& root) {
        root["ok"] = true;
        root["status"] = "deleted";
    });
}

} // namespace Handlers
} // namespace API
