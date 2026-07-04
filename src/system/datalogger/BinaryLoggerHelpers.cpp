#include "BinaryLoggerHelpers.h"
#include "BinaryFormat.h"
#include "../logging/Logging.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../../system/utils/ScopeLock.h"

#include <cstring>
#include <time.h>

#undef LOG_TAG
#define LOG_TAG "BinLog"

#define DATA_DIR "/data"

namespace DATALOG {

namespace {

constexpr size_t kMaxActiveReadLeases = 4;
constexpr TickType_t kReadLeaseLockTimeout = pdMS_TO_TICKS(100);

struct ActiveReadLease {
    char path[PATH_BUFFER_SIZE] = {0};
    uint8_t refCount = 0;
};

SemaphoreHandle_t s_readLeaseMutex = nullptr;
ActiveReadLease s_activeReadLeases[kMaxActiveReadLeases];

void ensureLeaseMutex() {
    if (!s_readLeaseMutex) {
        s_readLeaseMutex = xSemaphoreCreateMutex();
    }
}

int findLeaseSlotLocked(const char* path) {
    for (size_t i = 0; i < kMaxActiveReadLeases; i++) {
        if (s_activeReadLeases[i].refCount == 0) {
            continue;
        }
        if (strncmp(s_activeReadLeases[i].path, path, sizeof(s_activeReadLeases[i].path)) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int findFreeLeaseSlotLocked() {
    for (size_t i = 0; i < kMaxActiveReadLeases; i++) {
        if (s_activeReadLeases[i].refCount == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

}  // namespace

void BinaryLoggerHelpers::getMonthDir(char* out, size_t len) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char monthBuf[8];
    strftime(monthBuf, sizeof(monthBuf), "%Y-%m", &timeinfo);
    snprintf(out, len, "%s/%s", DATA_DIR, monthBuf);
}

void BinaryLoggerHelpers::getFilePath(char* out, size_t len) {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char monthBuf[8], dateBuf[11];
    strftime(monthBuf, sizeof(monthBuf), "%Y-%m", &timeinfo);
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &timeinfo);
    
    snprintf(out, len, "%s/%s/%s.bin", DATA_DIR, monthBuf, dateBuf);
}

bool BinaryLoggerHelpers::ensureDirectoryExists(const char* path) {
    if (LittleFS.exists(path)) {
        return true;
    }
    if (LittleFS.mkdir(path)) {
        LOGI("Created dir: %s", path);
        return true;
    }
    LOGE("Failed to create dir: %s", path);
    return false;
}

bool BinaryLoggerHelpers::acquireReadLease(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    bool acquired = false;
    bool tableFull = false;

    ensureLeaseMutex();
    SYSTEM::ScopeLock lock(s_readLeaseMutex, kReadLeaseLockTimeout);
    if (!lock.isLocked()) {
        LOGW("Read lease mutex timeout for: %s", path);
        return false;
    }

    const int existingSlot = findLeaseSlotLocked(path);
    if (existingSlot >= 0) {
        ActiveReadLease& slot = s_activeReadLeases[existingSlot];
        if (slot.refCount < UINT8_MAX) {
            slot.refCount++;
            acquired = true;
        }
    } else {
        const int freeSlot = findFreeLeaseSlotLocked();
        if (freeSlot >= 0) {
            ActiveReadLease& slot = s_activeReadLeases[freeSlot];
            strncpy(slot.path, path, sizeof(slot.path) - 1);
            slot.path[sizeof(slot.path) - 1] = '\0';
            slot.refCount = 1;
            acquired = true;
        } else {
            tableFull = true;
        }
    }

    lock.unlock();

    if (!acquired && tableFull) {
        LOGW("Read lease table full for: %s", path);
    }

    return acquired;
}

void BinaryLoggerHelpers::releaseReadLease(const char* path) {
    if (!path || path[0] == '\0') {
        return;
    }

    ensureLeaseMutex();
    SYSTEM::ScopeLock lock(s_readLeaseMutex, kReadLeaseLockTimeout);
    if (!lock.isLocked()) {
        LOGW("Read lease release mutex timeout for: %s", path);
        return;
    }

    const int slotIndex = findLeaseSlotLocked(path);
    if (slotIndex >= 0) {
        ActiveReadLease& slot = s_activeReadLeases[slotIndex];
        if (slot.refCount > 0) {
            slot.refCount--;
        }
        if (slot.refCount == 0) {
            slot.path[0] = '\0';
        }
    }
}

bool BinaryLoggerHelpers::isReadLeaseActive(const char* path) {
    if (!path || path[0] == '\0') {
        return false;
    }

    bool active = false;

    ensureLeaseMutex();
    SYSTEM::ScopeLock lock(s_readLeaseMutex, kReadLeaseLockTimeout);
    if (lock.isLocked()) {
        active = findLeaseSlotLocked(path) >= 0;
    } else {
        LOGW("Read lease check mutex timeout for: %s", path);
    }

    return active;
}

bool BinaryLoggerHelpers::validateFileHeader(File& file) {
    if (file.size() < sizeof(BinaryFileHeader)) {
        return false;
    }
    
    BinaryFileHeader header;
    file.seek(0);
    size_t bytesRead = file.read(reinterpret_cast<uint8_t*>(&header), sizeof(header));
    
    if (bytesRead != sizeof(header)) {
        return false;
    }
    
    if (header.magic != BINARY_MAGIC) {
        LOGE("Invalid magic: 0x%08X (expected 0x%08X)", header.magic, BINARY_MAGIC);
        return false;
    }
    
    if (header.version != BINARY_VERSION) {
        LOGW("Version mismatch: %d (expected %d)", header.version, BINARY_VERSION);
        return false;
    }
    
    if (header.recordSize != BINARY_RECORD_SIZE) {
        LOGE("Record size mismatch: %d (expected %d)", header.recordSize, BINARY_RECORD_SIZE);
        return false;
    }
    
    return true;
}

void BinaryLoggerHelpers::writeFileHeader(File& file) {
    BinaryFileHeader header;
    header.magic = BINARY_MAGIC;
    header.version = BINARY_VERSION;
    header.recordSize = BINARY_RECORD_SIZE;
    header.reserved = 0;
    
    file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header));
    LOGD("Wrote file header (magic=0x%08X, version=%d, recordSize=%d)", 
         header.magic, header.version, header.recordSize);
}

}  // namespace DATALOG
