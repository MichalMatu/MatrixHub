#pragma once

#include <Arduino.h>
#include <map>
#include <set>
#include <string>
#include <vector>

struct FsStubEntry {
    std::string name;
    bool isDir = false;
};

inline std::map<std::string, std::vector<FsStubEntry>>& fsStubDirs() {
    static std::map<std::string, std::vector<FsStubEntry>> dirs;
    return dirs;
}

inline std::set<std::string>& fsStubFiles() {
    static std::set<std::string> files;
    return files;
}

inline void fsStubReset() {
    fsStubDirs().clear();
    fsStubFiles().clear();
}

inline void fsStubSetDirEntries(const std::string& path, const std::vector<FsStubEntry>& entries) {
    fsStubDirs()[path] = entries;
}

inline void fsStubSetFileExists(const std::string& path, bool exists) {
    if (exists) {
        fsStubFiles().insert(path);
    } else {
        fsStubFiles().erase(path);
    }
}

namespace TEST_STUBS::FILESYSTEM {
inline bool beginResult = true;
inline bool formatResult = true;
inline int beginCalls = 0;
inline int endCalls = 0;
inline int formatCalls = 0;
inline std::vector<std::string> operations;

inline void reset() {
    beginResult = true;
    formatResult = true;
    beginCalls = 0;
    endCalls = 0;
    formatCalls = 0;
    operations.clear();
    fsStubReset();
}
}

class File : public Stream {
public:
    File() : _valid(false) {}
    explicit File(const std::string& name, bool isDir = false)
        : _valid(true), _isDir(isDir), _name(name) {}
    explicit File(const std::vector<FsStubEntry>* entries)
        : _valid(true), _isDir(true), _entries(entries), _index(0) {}
    virtual size_t write(uint8_t) { return 1; }
    virtual size_t write(const uint8_t* buf, size_t size) {
        (void)buf;
        return size;
    }
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    size_t read(uint8_t* buf, size_t size) { return 0; }
    virtual int peek() { return -1; }
    virtual void flush() {}
    virtual void close() {}
    operator bool() const { return _valid; }
    
    // Additional methods used by LittleFS
    String readString() { return ""; }
    size_t size() const { return 0; }
    const char* name() const { return _name.c_str(); }
    bool isDirectory() const { return _isDir; }
    File openNextFile() {
        if (!_entries || _index >= _entries->size()) return File();
        const auto& entry = (*_entries)[_index++];
        return File(entry.name, entry.isDir);
    }

private:
    bool _valid = false;
    bool _isDir = false;
    std::string _name;
    const std::vector<FsStubEntry>* _entries = nullptr;
    size_t _index = 0;
};

class FS {
public:
    bool begin(bool formatOnFail=false, const char * basePath="/littlefs", uint8_t maxOpenFiles=10) {
        (void)basePath;
        (void)maxOpenFiles;
        TEST_STUBS::FILESYSTEM::beginCalls++;
        TEST_STUBS::FILESYSTEM::operations.push_back(
            std::string("fs.begin:") + (formatOnFail ? "true" : "false"));
        return TEST_STUBS::FILESYSTEM::beginResult;
    }
    void end() {
        TEST_STUBS::FILESYSTEM::endCalls++;
        TEST_STUBS::FILESYSTEM::operations.push_back("fs.end");
    }
    bool format() {
        TEST_STUBS::FILESYSTEM::formatCalls++;
        TEST_STUBS::FILESYSTEM::operations.push_back("fs.format");
        return TEST_STUBS::FILESYSTEM::formatResult;
    }
    size_t totalBytes() { return 1024 * 1024; }
    size_t usedBytes() { return 0; }
    
    File open(const char* path, const char* mode = "r") {
        (void)mode;
        if (!path) return File();
        auto dirIt = fsStubDirs().find(path);
        if (dirIt != fsStubDirs().end()) {
            return File(&dirIt->second);
        }
        if (fsStubFiles().count(path)) {
            return File(path, false);
        }
        return File();
    }
    File open(const String& path, const char* mode = "r") {
        return open(path.c_str(), mode);
    }
    
    bool exists(const char* path) {
        if (!path) return false;
        if (fsStubDirs().count(path)) return true;
        if (fsStubFiles().count(path)) return true;
        return false;
    }
    bool exists(const String& path) { return exists(path.c_str()); }
    
    bool remove(const char* path) {
        if (!path) return false;
        fsStubFiles().erase(path);
        return true;
    }
    bool remove(const String& path) { return remove(path.c_str()); }
    
    bool rename(const char* pathFrom, const char* pathTo) {
        if (!pathFrom || !pathTo) return false;
        if (fsStubFiles().count(pathFrom)) {
            fsStubFiles().erase(pathFrom);
            fsStubFiles().insert(pathTo);
        }
        return true;
    }
    bool rename(const String& pathFrom, const String& pathTo) { return rename(pathFrom.c_str(), pathTo.c_str()); }
    
    bool mkdir(const char *path) {
        if (!path) return false;
        if (!fsStubDirs().count(path)) {
            fsStubDirs()[path] = {};
        }
        return true;
    }
    bool mkdir(const String &path) { return mkdir(path.c_str()); }
    
    bool rmdir(const char *path) { return true; }
    bool rmdir(const String &path) { return rmdir(path.c_str()); }
};

namespace fs {
using FS = ::FS;
}
