#include "MacroRepository.h"
#include "../../config/App.h"
#include "../../system/logging/Logging.h"
#include "../../system/utils/ScopeLock.h"
#include <cstring>


#undef LOG_TAG
#define LOG_TAG "MacroRepository"

namespace MACROS {


SemaphoreHandle_t MacroRepository::_fsMutex = nullptr;

void MacroRepository::setFsMutex(SemaphoreHandle_t mutex) {
    _fsMutex = mutex;
}



PsramVector<PsramString> MacroRepository::listScripts() {
    PsramVector<PsramString> files;
    
    SYSTEM::ScopeLock lock(_fsMutex ? _fsMutex : nullptr, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (_fsMutex && !lock.isLocked()) {
        LOGW("FS mutex timeout in listScripts");
        return files;
    }
    
    File root = LittleFS.open("/scripts");
    if (!root || !root.isDirectory()) {
        return files;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            const char* name = file.name();
            if (name && name[0] != '\0') {
                const char* base = name;
                if (strncmp(name, "/scripts/", 9) == 0) {
                    base = name + 9;
                }
                // Skip temp files that might remain after a failed save.
                size_t len = strlen(base);
                if (len >= 4 && strcmp(base + len - 4, ".tmp") == 0) {
                    file = root.openNextFile();
                    continue;
                }
                files.push_back(PsramString(base));
            }
        }
        file = root.openNextFile();
    }
    return files;
}

bool MacroRepository::deleteScript(const char* filename) {
    char path[128];
    snprintf(path, sizeof(path), "/scripts/%s", filename);
    
    SYSTEM::ScopeLock lock(_fsMutex ? _fsMutex : nullptr, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (_fsMutex && !lock.isLocked()) {
        LOGW("FS mutex timeout in deleteScript");
        return false;
    }
    
    bool result = false;
    if (LittleFS.exists(path)) {
        result = LittleFS.remove(path);
    }
    return result;
}

bool MacroRepository::saveScript(const char* filename, const char* content) {
    char path[128];
    snprintf(path, sizeof(path), "/scripts/%s", filename);
    char tmpPath[256];
    snprintf(tmpPath, sizeof(tmpPath), "/scripts/%s.tmp", filename);
    
    SYSTEM::ScopeLock lock(_fsMutex ? _fsMutex : nullptr, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (_fsMutex && !lock.isLocked()) {
        LOGW("FS mutex timeout in saveScript");
        return false;
    }
    
    if (!LittleFS.exists("/scripts")) {
        LittleFS.mkdir("/scripts");
    }
    
    File f = LittleFS.open(tmpPath, "w");
    if (!f) {
        LOGE("Failed to open tmp file for writing: %s", tmpPath);
        return false;
    }
    
    size_t contentLen = strlen(content);
    size_t written = f.print(content);
    f.close();
    
    if (written != contentLen) {
        LOGE("Incomplete write to %s: wrote %zu of %u bytes (disk full?)", 
             tmpPath, written, (unsigned)contentLen);
        LittleFS.remove(tmpPath);
        return false;
    }
    
    // Atomic Swap
    if (LittleFS.exists(path)) {
        LittleFS.remove(path);
    }
    
    if (!LittleFS.rename(tmpPath, path)) {
        LOGE("Failed to rename atomic save: %s", path);
        LittleFS.remove(tmpPath);
        return false;
    }
    return true;
}

PsramString MacroRepository::getScriptContent(const char* filename) {
    char path[128];
    snprintf(path, sizeof(path), "/scripts/%s", filename);
    
    SYSTEM::ScopeLock lock(_fsMutex ? _fsMutex : nullptr, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (_fsMutex && !lock.isLocked()) {
        LOGW("FS mutex timeout in getScriptContent");
        return "";
    }
    
    if (!LittleFS.exists(path)) {
        return "";
    }
    
    File f = LittleFS.open(path, "r");
    if (!f) {
        return "";
    }
    
    size_t size = f.size();
    if (size == 0) {
        f.close();
        return "";
    }

    // Allocate buffer in PSRAM to avoid DRAM fragmentation
    PsramString psContent;
    
    // Safety: Check if we have enough PSRAM before attempting alloc
    if (size > heap_caps_get_free_size(MALLOC_CAP_SPIRAM)) {
         LOGE("Not enough PSRAM for script content: %u needed", (unsigned)size);
         f.close();
         return "";
    }

    try {
        psContent.resize(size);
    } catch (...) {
        LOGE("Failed to allocate PSRAM for script content");
        f.close();
        return "";
    }

    f.read((uint8_t*)psContent.data(), size);
    f.close();
    
    return psContent;
}

bool MacroRepository::scriptExists(const char* filename) {
    char path[128];
    snprintf(path, sizeof(path), "/scripts/%s", filename);

    SYSTEM::ScopeLock lock(_fsMutex ? _fsMutex : nullptr, pdMS_TO_TICKS(API::FS_MUTEX_TIMEOUT_MS));
    if (_fsMutex && !lock.isLocked()) {
        LOGW("FS mutex timeout in scriptExists");
        return false;
    }

    return LittleFS.exists(path);
}

} // namespace MACROS
