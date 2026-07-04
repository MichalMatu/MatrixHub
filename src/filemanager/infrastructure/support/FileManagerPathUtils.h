#pragma once

#include <Arduino.h>
#include <stdint.h>
#include "system/memory/PsramAllocator.h"

namespace FILEMGR {
namespace FileManagerPathUtils {

enum class FileManagerPathAccess : uint8_t {
  List,
  Download,
  Upload,
  Remove,
};

String sanitizeFilename(const String& filename);
SYSTEM::PsramString sanitizeFilename(const SYSTEM::PsramString& filename);
SYSTEM::PsramString sanitizeFilename(const char* filename);
bool canonicalizeAbsolutePath(const String& path, const String& fallback, String& canonicalPath);
bool canonicalizeAbsolutePath(
    const SYSTEM::PsramString& path,
    const SYSTEM::PsramString& fallback,
    SYSTEM::PsramString& canonicalPath
);
bool canonicalizeAbsolutePath(const char* path, const char* fallback, SYSTEM::PsramString& canonicalPath);
bool canonicalizeAbsoluteDirectory(const String& path, const String& fallback, String& canonicalPath);
bool canonicalizeAbsoluteDirectory(
    const SYSTEM::PsramString& path,
    const SYSTEM::PsramString& fallback,
    SYSTEM::PsramString& canonicalPath
);
bool canonicalizeAbsoluteDirectory(const char* path, const char* fallback, SYSTEM::PsramString& canonicalPath);
String ensureAbsoluteDirectory(const String& path, const String& fallback = "/");
SYSTEM::PsramString ensureAbsoluteDirectory(
    const SYSTEM::PsramString& path,
    const SYSTEM::PsramString& fallback
);
SYSTEM::PsramString ensureAbsoluteDirectory(const char* path, const char* fallback = "/");
String joinPaths(const String& baseDirectory, const String& childName);
SYSTEM::PsramString joinPaths(
    const SYSTEM::PsramString& baseDirectory,
    const SYSTEM::PsramString& childName
);
SYSTEM::PsramString joinPaths(const char* baseDirectory, const char* childName);
String buildFullPath(const String& baseDirectory, const String& relativePath);
SYSTEM::PsramString buildFullPath(
    const SYSTEM::PsramString& baseDirectory,
    const SYSTEM::PsramString& relativePath
);
SYSTEM::PsramString buildFullPath(const char* baseDirectory, const char* relativePath);
bool isAccessAllowed(const String& canonicalNativePath, FileManagerPathAccess access);
bool isAccessAllowed(const SYSTEM::PsramString& canonicalNativePath, FileManagerPathAccess access);
bool isAccessAllowed(const char* canonicalNativePath, FileManagerPathAccess access);

}  // namespace FileManagerPathUtils
}  // namespace FILEMGR
