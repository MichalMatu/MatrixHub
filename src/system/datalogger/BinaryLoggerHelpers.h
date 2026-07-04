#pragma once

#include <Arduino.h>
#include <LittleFS.h>

namespace DATALOG {

// Buffer size constants to avoid magic numbers
constexpr size_t PATH_BUFFER_SIZE = 48;
constexpr size_t MONTH_DIR_SIZE = 16;

/**
 * Helper functions for binary logger file operations.
 * All path functions use output buffers to avoid String/heap allocation.
 */
class BinaryLoggerHelpers {
public:
    // Path generation - writes to caller-provided buffer
    static void getMonthDir(char* out, size_t len);
    static void getFilePath(char* out, size_t len);
    static bool ensureDirectoryExists(const char* path);

    // Active-read leases protect streamed log files from delete/rotation
    // without forcing long-lived LittleFS mutex holds across network I/O.
    static bool acquireReadLease(const char* path);
    static void releaseReadLease(const char* path);
    static bool isReadLeaseActive(const char* path);
    
    // Header operations
    static bool validateFileHeader(File& file);
    static void writeFileHeader(File& file);
};

}  // namespace DATALOG
