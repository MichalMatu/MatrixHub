#include "BinaryDataLogger.h"
#include "BinaryFormat.h"
#include "BinaryLoggerHelpers.h"
#include "../../config/System.h"
#include "../../core/config/ConfigCommon.h"
#include "../logging/Logging.h"
#include "../utils/ScopeLock.h"
#include "../watchdog/TaskWatchdog.h"

#include <time.h>

#undef LOG_TAG
#define LOG_TAG "BinLog"

#define DATA_DIR "/data"

namespace DATALOG {

namespace {
    int s_lastLoggedDayId = -1;
    bool s_headerVerified = false;
    bool s_fsReady = false;
    // Deferred low-space signal. The hot append path sets this instead of
    // walking the entire filesystem inline while holding up unrelated FS work.
    bool s_rotationPending = false;

    // Rotation and append can walk a lot of LittleFS metadata when storage is
    // near full. Keep the current task alive while these cold-path operations run.
    inline void feedTaskWatchdog() {
        (void)SYSTEM::TaskWatchdog::instance().reset();
    }

    const char* childEntryPath(const File& entry) {
        // openNextFile() is inconsistent across our LittleFS stack: some paths
        // come back as "/data/2026-04", others as just "2026-04". Rotation
        // needs one canonical absolute path so delete/read-lease checks never
        // build malformed strings like "/data//data/2026-04".
        const char* fullPath = entry.path();
        if (fullPath && fullPath[0] == '/') {
            return fullPath;
        }
        return entry.name();
    }

    bool copyChildEntryPath(const char* parentPath, const File& entry, char* outPath, size_t outPathLen) {
        if (!outPath || outPathLen == 0) {
            return false;
        }
        outPath[0] = '\0';

        const char* rawPath = childEntryPath(entry);
        if (!rawPath || rawPath[0] == '\0') {
            return false;
        }

        if (rawPath[0] == '/') {
            snprintf(outPath, outPathLen, "%s", rawPath);
        } else {
            snprintf(outPath, outPathLen, "%s/%s", parentPath ? parentPath : "", rawPath);
        }

        return outPath[0] != '\0';
    }

    size_t getFreeBytesLocked() {
        const size_t total = LittleFS.totalBytes();
        const size_t used = LittleFS.usedBytes();
        return (used <= total) ? (total - used) : 0;
    }

    void buildCurrentLogPath(char* outFullpath, size_t outFullpathLen, char* outMonthDir = nullptr, size_t outMonthDirLen = 0, int* outDayId = nullptr) {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        // Intentional: file layout always follows the current system clock,
        // even before SNTP becomes valid. In AP/offline deployments this keeps
        // datalogging fully active without introducing a second "timeless"
        // storage mode. Consumers and maintenance flows are expected to accept
        // pre-NTP directories/files as a normal transient condition.
        char monthBuf[8];
        char dateBuf[11];
        strftime(monthBuf, sizeof(monthBuf), "%Y-%m", &timeinfo);
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &timeinfo);

        if (outDayId) {
            *outDayId = (timeinfo.tm_year << 16) | timeinfo.tm_yday;
        }
        if (outMonthDir && outMonthDirLen > 0) {
            snprintf(outMonthDir, outMonthDirLen, "%s/%s", DATA_DIR, monthBuf);
        }
        snprintf(outFullpath, outFullpathLen, "%s/%s/%s.bin", DATA_DIR, monthBuf, dateBuf);
    }

    void yieldDuringRotateScan() {
        feedTaskWatchdog();
        taskYIELD();
    }

    void pauseAfterRotateDelete() {
        feedTaskWatchdog();
        if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
            vTaskDelay(pdMS_TO_TICKS(DATALOG::ROTATE_DELETE_PAUSE_MS));
        } else {
            taskYIELD();
        }
        feedTaskWatchdog();
    }

    bool closeFileLocked(File& file) {
        if (!file) {
            return true;
        }

        SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (g_fsMutex && !lock.isLocked()) {
            return false;
        }

        file.close();
        return true;
    }

    // Fast-path helper caching the current day directory and header status in RAM.
    // Prevents Task Starvation caused by hitting VFS (LittleFS.exists/open) on every single sensor value logged.
    // Caller must already hold the global filesystem mutex when one exists.
    bool ensureDailyFileReadyLocked(char* outFullpath, size_t outFullpathLen) {
        int currentDayId = -1;
        char monthDir[MONTH_DIR_SIZE];
        buildCurrentLogPath(outFullpath, outFullpathLen, monthDir, sizeof(monthDir), &currentDayId);

        if (s_lastLoggedDayId == currentDayId && s_headerVerified) {
            // The fast path only caches "today's file was prepared" in RAM. The
            // file itself can still disappear later via Log UI delete/cleanup,
            // so re-check existence before we skip header preparation. Without
            // this guard the next append could recreate the file in "a" mode and
            // write the first record without a binary header.
            if (LittleFS.exists(outFullpath)) {
                return true;
            }
            s_headerVerified = false;
        }

        // Slow path: day changed or first run
        if (!BinaryLoggerHelpers::ensureDirectoryExists(DATA_DIR)) {
            LOGE("Data dir unavailable: %s", DATA_DIR);
            return false;
        }
        feedTaskWatchdog();
        if (!BinaryLoggerHelpers::ensureDirectoryExists(monthDir)) {
            LOGE("Failed to create month dir: %s", monthDir);
            return false;
        }
        feedTaskWatchdog();

        const bool isNewFile = !LittleFS.exists(outFullpath);
        feedTaskWatchdog();
        if (isNewFile) {
            File file = LittleFS.open(outFullpath, "w");
            if (file) {
                BinaryLoggerHelpers::writeFileHeader(file);
                file.close();
                feedTaskWatchdog();
                LOGI("Created today's log file with header: %s", outFullpath);
                s_lastLoggedDayId = currentDayId;
                s_headerVerified = true;
                return true;
            }
            LOGE("Failed to create today's log file: %s", outFullpath);
            return false;
        } else {
            // Existing file path: validate the actual binary header, not just the
            // file length. The charts/logs UI rejects bad magic/version/recordSize,
            // so the writer must not keep appending to a file whose header only
            // "looks present" because it is 8+ bytes long.
            File check = LittleFS.open(outFullpath, "r");
            if (!check) {
                // Existing file but unreadable means we cannot trust the cached
                // "header ready" fast path for this day. Force the caller to
                // treat the append as failed instead of silently writing into a
                // file whose header state is unknown.
                LOGE("Failed to open existing log file for verification: %s", outFullpath);
                return false;
            }

            const size_t existingSize = check.size();
            const bool headerValid = BinaryLoggerHelpers::validateFileHeader(check);
            if (!headerValid) {
                check.close();
                const bool preservePayload = existingSize >= sizeof(BinaryFileHeader);
                File fw = LittleFS.open(outFullpath, preservePayload ? "r+" : "w");
                if (!fw) {
                    // Do not mark the day as header-verified if recovery fails.
                    // Otherwise later appends can treat a broken file as healthy
                    // and keep extending a log that the frontend parser rejects.
                    LOGE("Failed to repair invalid log header: %s", outFullpath);
                    return false;
                }

                if (preservePayload) {
                    // r+ keeps any existing records and only refreshes the first
                    // 8 bytes of metadata. We only fall back to truncating "w"
                    // when the file is already too short to contain records.
                    fw.seek(0);
                }

                BinaryLoggerHelpers::writeFileHeader(fw);
                fw.close();
                feedTaskWatchdog();
                LOGI("Fixed invalid header in log file: %s", outFullpath);
            } else {
                check.close();
            }
            feedTaskWatchdog();
            s_lastLoggedDayId = currentDayId;
            s_headerVerified = true;
            return true;
        }
    }
}

void BinaryDataLogger::begin() {
    s_fsReady = false;
    s_lastLoggedDayId = -1;
    s_headerVerified = false;
    s_rotationPending = false;

    // Filesystem mount lifecycle belongs to StorageInitializer.
    // Datalogger only attaches to an already mounted LittleFS instance.
    {
        SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (g_fsMutex && !lock.isLocked()) {
            LOGE("LittleFS mutex busy; BinaryDataLogger disabled");
            return;
        }

        File probe = LittleFS.open("/");
        if (!probe) {
            LOGE("LittleFS unavailable; BinaryDataLogger disabled");
            return;
        }
        probe.close();
    }

    s_fsReady = true;
    LOGI("BinaryDataLogger attached to mounted LittleFS");

    char filepath[PATH_BUFFER_SIZE];
    buildCurrentLogPath(filepath, sizeof(filepath));
    if (!checkRotate(sizeof(BinaryFileHeader), filepath)) {
        LOGW("Storage still below target after boot rotation");
    }

    // Create today's file with header if it doesn't exist yet
    // This prevents API crashes when UI requests chart data right after midnight
    SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (!g_fsMutex || lock.isLocked()) {
        ensureDailyFileReadyLocked(filepath, sizeof(filepath));
    }
}

BinaryLogWriteResult BinaryDataLogger::logSensorData(uint16_t co2, float temp, float humid) {
    if (!s_fsReady) {
        return BinaryLogWriteResult::Error;
    }

    // Validate inputs
    if (co2 == 0 || co2 >= 65535) {
        LOGW("Invalid CO2 value: %u, skipping log", co2);
        return BinaryLogWriteResult::Error;
    }

    char filepath[PATH_BUFFER_SIZE];
    buildCurrentLogPath(filepath, sizeof(filepath));
    const size_t payloadBytes = sizeof(BinaryLogRecord);

    SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (g_fsMutex && !lock.isLocked()) {
        return BinaryLogWriteResult::Busy;
    }

    // Intended behavior: the append attempt stays short and predictable. If we
    // do not have the free-space headroom for a safe write, we schedule
    // maintenance and exit instead of running rotate/cleanup under this same
    // append lock window.
    const size_t headerBytes = LittleFS.exists(filepath) ? 0 : sizeof(BinaryFileHeader);
    const size_t bytesNeeded = payloadBytes + headerBytes;
    const size_t free = getFreeBytesLocked();
    const size_t targetFree = DATALOG::MIN_FREE_SPACE_BYTES + bytesNeeded;
    feedTaskWatchdog();

    if (free < targetFree) {
        s_rotationPending = true;
        LOGW("Low storage before sensor write: %lu B free, target %lu B. Scheduling maintenance.",
             static_cast<unsigned long>(free),
             static_cast<unsigned long>(targetFree));
        return BinaryLogWriteResult::NeedsMaintenance;
    }

    if (!ensureDailyFileReadyLocked(filepath, sizeof(filepath))) {
        return BinaryLogWriteResult::Error;
    }
    feedTaskWatchdog();

    File file = LittleFS.open(filepath, "a");
    if (!file) {
        LOGE("Failed to open: %s", filepath);
        return BinaryLogWriteResult::Error;
    }
    feedTaskWatchdog();

    // Note: File opened in append mode ("a") is already at EOF position
    // No need to seek or validate on every write (pre-release, trusted FS)

    // Prepare binary record
    BinaryLogRecord record;
    record.timestamp = static_cast<uint32_t>(time(nullptr));
    record.co2 = co2;
    record.temp_10x = floatToInt16_10x(temp);
    record.humid_10x = floatToUInt16_10x(humid);

    // Write binary record with profiling
    LOG_PROFILE_START(writeStart);
    size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(&record), sizeof(record));
    file.close();
    feedTaskWatchdog();
    LOG_PROFILE_END_SMART(writeStart, "Flash write", TASK_MONITOR::INTERVAL_FLASH_WRITE_MS, TASK_MONITOR::THRESHOLD_FLASH_WRITE_US);

    if (bytesWritten == sizeof(record)) {
        s_rotationPending = false;
        LOGD("Logged to: %s (ts=%lu, co2=%u, temp=%d, humid=%u)",
             filepath, record.timestamp, record.co2, record.temp_10x, 
             record.humid_10x);
        return BinaryLogWriteResult::Success;
    }

    LOGE("Write failed: wrote %d bytes (expected %d)", bytesWritten, sizeof(record));
    return BinaryLogWriteResult::Error;
}

void BinaryDataLogger::logBatch(const BinaryLogRecord* records, size_t count) {
    if (!s_fsReady) return;
    if (!records || count == 0) return;

    char filepath[PATH_BUFFER_SIZE];
    buildCurrentLogPath(filepath, sizeof(filepath));
    SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (g_fsMutex && !lock.isLocked()) {
        LOGW("Skipping batch write: filesystem mutex busy");
        return;
    }

    const size_t payloadBytes = count * sizeof(BinaryLogRecord);
    const size_t headerBytes = LittleFS.exists(filepath) ? 0 : sizeof(BinaryFileHeader);
    const size_t bytesNeeded = payloadBytes + headerBytes;
    const size_t free = getFreeBytesLocked();
    const size_t targetFree = DATALOG::MIN_FREE_SPACE_BYTES + bytesNeeded;
    feedTaskWatchdog();
    if (free < targetFree) {
        s_rotationPending = true;
        LOGE("Insufficient storage for batch write: %s", filepath);
        return;
    }
    feedTaskWatchdog();

    if (!ensureDailyFileReadyLocked(filepath, sizeof(filepath))) {
        return;
    }
    feedTaskWatchdog();
    
    File file = LittleFS.open(filepath, "a");
    if (!file) {
        LOGE("Failed to open for batch: %s", filepath);
        return;
    }
    feedTaskWatchdog();
    
    size_t bytesWritten = file.write(reinterpret_cast<const uint8_t*>(records), count * sizeof(BinaryLogRecord));
    file.close();
    feedTaskWatchdog();
    
    if (bytesWritten == count * sizeof(BinaryLogRecord)) {
        s_rotationPending = false;
        LOGI("Batch logged %u records to %s", count, filepath);
    } else {
        LOGE("Batch write failed: wrote %d bytes (expected %d)", bytesWritten, count * sizeof(BinaryLogRecord));
    }
}

bool BinaryDataLogger::serviceStorageMaintenance(size_t bytesNeeded, const char* excludePath) {
    if (!s_fsReady) {
        return false;
    }

    // No-op fast path: most writes should not pay for rotation checks once the
    // filesystem has enough headroom again.
    if (!s_rotationPending) {
        return true;
    }

    // Slow path: perform the previously deferred cleanup. Callers are expected
    // to do this outside the short append attempt, then retry the append once.
    const bool rotated = checkRotate(bytesNeeded, excludePath);
    if (rotated) {
        s_rotationPending = false;
    }
    return rotated;
}

bool BinaryDataLogger::checkRotate(size_t bytesNeeded, const char* excludePath) {
    if (!s_fsReady) {
        return false;
    }

    feedTaskWatchdog();

    // Rotation still does the same logical work as before, but it no longer
    // depends on one outer lock held by SensorBinaryLogger for the entire scan
    // and delete sequence. Each FS step takes the mutex briefly, so dashboard
    // reads and other FS clients can make progress between rotation steps.
    size_t total = 0;
    size_t used = 0;
    size_t free = 0;
    const size_t targetFree = DATALOG::MIN_FREE_SPACE_BYTES + bytesNeeded;

    {
        SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (g_fsMutex && !lock.isLocked()) {
            LOGW("Storage rotation skipped: filesystem mutex busy");
            return false;
        }

        total = LittleFS.totalBytes();
        used = LittleFS.usedBytes();
        free = getFreeBytesLocked();
    }

    LOGI("Storage: %lu KB total, %lu KB used, %lu KB free, target %lu KB",
         total / 1024, used / 1024, free / 1024, targetFree / 1024);

    if (free >= targetFree) {
        s_rotationPending = false;
        return true;
    }

    bool dataDirExists = false;
    {
        SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
        if (g_fsMutex && !lock.isLocked()) {
            LOGW("Storage rotation skipped: data dir check blocked by busy mutex");
            return false;
        }

        dataDirExists = LittleFS.exists(DATA_DIR);
    }

    if (!dataDirExists) {
        LOGW("Data dir missing; cannot rotate log files");
        return false;
    }

    // Rotation strategy:
    // 1. walk /data month-by-month and file-by-file using short mutex windows
    // 2. pick the oldest eligible file by lexicographic path order
    //    (/data/YYYY-MM/YYYY-MM-DD.bin sorts chronologically)
    // 3. skip the current target file and any file protected by an active
    //    read lease so API streaming does not race with deletion
    // 4. delete one file, optionally prune an empty month dir, then re-check
    //    free space before deciding whether another deletion is needed
    //
    // This keeps the cleanup incremental and cooperative: we still reclaim
    // space deterministically, but other FS users get chances to acquire the
    // mutex between scan/delete steps instead of waiting behind one long hold.
    //
    // Wear leveling note: At 60s write interval (~1440 writes/day, 10-byte
    // records), flash endurance is ~19 years. Safe for typical sensor logging
    // use cases.
    while (free < targetFree) {
        feedTaskWatchdog();
        char oldestFilePath[PATH_BUFFER_SIZE] = {0};  // Full path for deletion
        char oldestDirPath[MONTH_DIR_SIZE] = {0};     // Full path for rmdir

        // Scan all month directories to find the oldest file
        File root;
        {
            SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            if (g_fsMutex && !lock.isLocked()) {
                LOGW("Storage rotation paused: filesystem mutex busy before opening data dir");
                return false;
            }

            root = LittleFS.open(DATA_DIR);
        }

        if (!root) {
            LOGE("Failed to open data dir");
            return false;
        }

        while (true) {
            File monthDir;
            char monthPath[MONTH_DIR_SIZE] = {0};
            bool hasMonth = false;

            {
                SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
                if (g_fsMutex && !lock.isLocked()) {
                    closeFileLocked(root);
                    LOGW("Storage rotation paused: filesystem mutex busy during month scan");
                    return false;
                }

                monthDir = root.openNextFile();
                if (!monthDir) {
                    break;
                }

                if (monthDir.isDirectory()) {
                    // Rotation historically built DATA_DIR + entry.name(), but
                    // openNextFile() may already return "/data/2026-04". Build
                    // one canonical absolute path before we reopen the month
                    // directory, compare leases, or delete old files.
                    hasMonth = copyChildEntryPath(DATA_DIR, monthDir, monthPath, sizeof(monthPath));
                }

                monthDir.close();
            }

            if (!hasMonth) {
                yieldDuringRotateScan();
                continue;
            }

            File monthHandle;
            {
                SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
                if (g_fsMutex && !lock.isLocked()) {
                    closeFileLocked(root);
                    LOGW("Storage rotation paused: filesystem mutex busy before opening month dir");
                    return false;
                }

                monthHandle = LittleFS.open(monthPath, "r");
            }

            if (!monthHandle || !monthHandle.isDirectory()) {
                closeFileLocked(monthHandle);
                yieldDuringRotateScan();
                continue;
            }

            while (true) {
                char filePath[PATH_BUFFER_SIZE] = {0};
                bool hasDayFile = false;

                {
                    SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
                    if (g_fsMutex && !lock.isLocked()) {
                        closeFileLocked(monthHandle);
                        closeFileLocked(root);
                        LOGW("Storage rotation paused: filesystem mutex busy during file scan");
                        return false;
                    }

                    File dayFile = monthHandle.openNextFile();
                    if (!dayFile) {
                        break;
                    }

                    if (!dayFile.isDirectory()) {
                        // Same normalization rule for day files: rotation,
                        // excludePath, and read-lease checks all expect the
                        // exact absolute "/data/..." path.
                        hasDayFile = copyChildEntryPath(monthPath, dayFile, filePath, sizeof(filePath));
                    }

                    dayFile.close();
                }

                if (hasDayFile) {
                    const bool isExcluded = excludePath && strcmp(filePath, excludePath) == 0;
                    const bool isInUse = BinaryLoggerHelpers::isReadLeaseActive(filePath);
                    if (!isExcluded &&
                        !isInUse &&
                        (oldestFilePath[0] == '\0' || strcmp(filePath, oldestFilePath) < 0)) {
                        strncpy(oldestFilePath, filePath, sizeof(oldestFilePath) - 1);
                        strncpy(oldestDirPath, monthPath, sizeof(oldestDirPath) - 1);
                        oldestFilePath[sizeof(oldestFilePath) - 1] = '\0';
                        oldestDirPath[sizeof(oldestDirPath) - 1] = '\0';
                    }
                }

                yieldDuringRotateScan();
            }

            closeFileLocked(monthHandle);
            yieldDuringRotateScan();
        }

        closeFileLocked(root);

        // No files found - nothing to delete
        if (oldestFilePath[0] == '\0') {
            LOGW("No eligible files to delete for rotation");
            return false;
        }

        // Delete the oldest file using full path
        LOGW("Deleting oldest file: %s", oldestFilePath);
        {
            SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            if (g_fsMutex && !lock.isLocked()) {
                LOGW("Storage rotation paused: filesystem mutex busy before delete");
                return false;
            }

            if (LittleFS.remove(oldestFilePath)) {
                LOGI("Deleted: %s", oldestFilePath);
            } else {
                LOGE("Failed to delete: %s", oldestFilePath);
                return false;  // Avoid infinite loop
            }
        }
        feedTaskWatchdog();

        // Check if the month directory is now empty and remove it
        {
            SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            if (g_fsMutex && !lock.isLocked()) {
                LOGW("Storage rotation paused: filesystem mutex busy during empty-dir cleanup");
                return false;
            }

            File checkDir = LittleFS.open(oldestDirPath, "r");
            if (checkDir) {
                File remaining = checkDir.openNextFile();
                const bool isEmpty = !remaining;
                if (remaining) {
                    remaining.close();
                }
                checkDir.close();

                if (isEmpty) {
                    if (LittleFS.rmdir(oldestDirPath)) {
                        LOGI("Removed empty dir: %s", oldestDirPath);
                    }
                }
            }
        }
        feedTaskWatchdog();

        pauseAfterRotateDelete();

        // Recalculate free space
        {
            SYSTEM::ScopeLock lock(g_fsMutex, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
            if (g_fsMutex && !lock.isLocked()) {
                LOGW("Storage rotation paused: filesystem mutex busy during free-space refresh");
                return false;
            }

            used = LittleFS.usedBytes();
            free = getFreeBytesLocked();
        }
        feedTaskWatchdog();
        LOGD("After deletion: %lu KB free", free / 1024);
    }

    s_rotationPending = free < targetFree;
    return free >= targetFree;
}

}  // namespace DATALOG
