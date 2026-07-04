#include "filemanager/infrastructure/support/FileManagerPathUtils.h"

#include <ctype.h>
#include <pgmspace.h>
#include <cstring>
#include <string_view>

namespace FILEMGR {
namespace FileManagerPathUtils {
namespace {
using PathView = std::string_view;
using PsramString = SYSTEM::PsramString;

constexpr size_t kLeadingSlashExtra = 1;
constexpr char kPathSeparator = '/';
constexpr char kWindowsSeparator = '\\';
constexpr const char* kRootPath = "/";
constexpr const char* kProtectedConfigRoot = "/config";
constexpr const char* kProtectedLogRoot = "/data";
const char kFallbackUploadName[] PROGMEM = "upload.bin";

PathView makeView(const char* text) {
  return text ? PathView(text) : PathView();
}

PathView makeView(const PsramString& text) {
  return PathView(text.data(), text.size());
}

PathView trimView(PathView text) {
  size_t start = 0;
  while (start < text.size() && isspace(static_cast<unsigned char>(text[start]))) {
    ++start;
  }

  size_t end = text.size();
  while (end > start && isspace(static_cast<unsigned char>(text[end - 1]))) {
    --end;
  }

  return text.substr(start, end - start);
}

PsramString trimCopy(PathView text) {
  const PathView trimmed = trimView(text);
  return PsramString(trimmed.data(), trimmed.size(), SYSTEM::PsramAllocator<char>());
}

PsramString ensureFallback(PathView path, PathView fallback) {
  PsramString normalized = trimCopy(path);
  if (!normalized.empty()) {
    return normalized;
  }

  normalized = trimCopy(fallback);
  if (!normalized.empty()) {
    return normalized;
  }

  return SYSTEM::makePsramString(kRootPath);
}

PathView stripLeadingSlashes(PathView path) {
  size_t start = 0;
  while (start < path.size() && path[start] == kPathSeparator) {
    ++start;
  }
  return path.substr(start);
}

bool startsWithPathPrefix(PathView path, const char* prefix) {
  const size_t prefixLength = strlen(prefix);
  if (path.size() < prefixLength || memcmp(path.data(), prefix, prefixLength) != 0) {
    return false;
  }

  return path.size() == prefixLength || path[prefixLength] == kPathSeparator;
}

bool isCurrentDirectorySegment(PathView segment) {
  return segment.size() == 1 && segment[0] == '.';
}

bool isParentDirectorySegment(PathView segment) {
  return segment.size() == 2 && segment[0] == '.' && segment[1] == '.';
}

bool hasInvalidSegmentChars(PathView segment) {
  for (size_t i = 0; i < segment.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(segment[i]);
    if (ch == static_cast<unsigned char>(kWindowsSeparator) || iscntrl(ch)) {
      return true;
    }
  }
  return false;
}

SYSTEM::PsramString makeFallbackUploadNamePsram()
{
  return SYSTEM::makePsramString(FPSTR(kFallbackUploadName));
}

String toArduinoString(const PsramString& text) {
  return text.empty() ? String() : String(text.c_str());
}

bool appendCanonicalSegments(PathView path, PsramString& canonicalPath, bool& hasSegment) {
  size_t cursor = !path.empty() && path[0] == kPathSeparator ? 1 : 0;
  while (cursor <= path.size()) {
    const size_t slashPos = path.find(kPathSeparator, cursor);
    const size_t segmentEnd = slashPos == PathView::npos ? path.size() : slashPos;
    const PathView segment = path.substr(cursor, segmentEnd - cursor);

    if (!segment.empty() && !isCurrentDirectorySegment(segment)) {
      if (isParentDirectorySegment(segment) || hasInvalidSegmentChars(segment)) {
        return false;
      }

      if (hasSegment) {
        canonicalPath.push_back(kPathSeparator);
      }
      canonicalPath.append(segment.data(), segment.size());
      hasSegment = true;
    }

    if (slashPos == PathView::npos) {
      break;
    }
    cursor = slashPos + 1;
  }

  return true;
}

bool canonicalizePathImpl(PathView rawPath, PathView fallback, PsramString& canonicalPath) {
  const PsramString candidate = ensureFallback(rawPath, fallback);
  const PathView candidateView = makeView(candidate);
  canonicalPath = PsramString();
  canonicalPath.reserve(candidateView.size() + kLeadingSlashExtra);
  canonicalPath.push_back(kPathSeparator);

  bool hasSegment = false;
  if (!appendCanonicalSegments(candidateView, canonicalPath, hasSegment)) {
    canonicalPath = PsramString();
    return false;
  }

  if (!hasSegment) {
    canonicalPath.assign(kRootPath);
  }
  return true;
}

PsramString sanitizeFilenameImpl(PathView filename) {
  if (filename.empty()) {
    return makeFallbackUploadNamePsram();
  }

  PsramString clean;
  clean.reserve(filename.size());
  for (const char c : filename) {
    if (isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_') {
      clean.push_back(c);
    } else {
      clean.push_back('_');
    }
  }

  size_t pos = 0;
  while ((pos = clean.find("..", pos)) != PsramString::npos) {
    clean.replace(pos, 2, "_");
    pos += 1;
  }

  if (clean.empty()) {
    return makeFallbackUploadNamePsram();
  }

  return clean;
}

PsramString joinPathsImpl(PathView baseDirectory, PathView childName) {
  PsramString canonicalBase;
  if (!canonicalizePathImpl(baseDirectory, makeView(kRootPath), canonicalBase)) {
    return PsramString();
  }

  const PathView trimmedChild = stripLeadingSlashes(trimView(childName));
  if (trimmedChild.empty()) {
    return canonicalBase;
  }

  bool hasSegment = canonicalBase.size() > 1;
  canonicalBase.reserve(canonicalBase.size() + trimmedChild.size() + kLeadingSlashExtra);
  if (!appendCanonicalSegments(trimmedChild, canonicalBase, hasSegment)) {
    return PsramString();
  }

  if (!hasSegment) {
    canonicalBase.assign(kRootPath);
  }
  return canonicalBase;
}

bool isAccessAllowedImpl(PathView canonicalNativePath, const FileManagerPathAccess access) {
  if (canonicalNativePath.empty() || canonicalNativePath[0] != kPathSeparator) {
    return false;
  }

  const bool isProtectedConfigPath = startsWithPathPrefix(canonicalNativePath, kProtectedConfigRoot);
  const bool isProtectedLogPath = startsWithPathPrefix(canonicalNativePath, kProtectedLogRoot);
  switch (access) {
    case FileManagerPathAccess::List:
    case FileManagerPathAccess::Download:
      return true;
    case FileManagerPathAccess::Upload:
      return !isProtectedConfigPath && !isProtectedLogPath;
    case FileManagerPathAccess::Remove:
      return !isProtectedConfigPath;
  }

  return false;
}
}  // namespace

String sanitizeFilename(const String& filename) {
  return toArduinoString(sanitizeFilenameImpl(makeView(filename.c_str())));
}

SYSTEM::PsramString sanitizeFilename(const SYSTEM::PsramString& filename) {
  return sanitizeFilenameImpl(makeView(filename));
}

SYSTEM::PsramString sanitizeFilename(const char* filename) {
  return sanitizeFilenameImpl(makeView(filename));
}

bool canonicalizeAbsolutePath(const String& path, const String& fallback, String& canonicalPath) {
  PsramString psramCanonicalPath;
  if (!canonicalizeAbsolutePath(path.c_str(), fallback.c_str(), psramCanonicalPath)) {
    canonicalPath = String();
    return false;
  }

  canonicalPath = toArduinoString(psramCanonicalPath);
  return true;
}

bool canonicalizeAbsolutePath(
    const SYSTEM::PsramString& path,
    const SYSTEM::PsramString& fallback,
    SYSTEM::PsramString& canonicalPath
) {
  return canonicalizePathImpl(makeView(path), makeView(fallback), canonicalPath);
}

bool canonicalizeAbsolutePath(const char* path, const char* fallback, SYSTEM::PsramString& canonicalPath) {
  return canonicalizePathImpl(makeView(path), makeView(fallback), canonicalPath);
}

bool canonicalizeAbsoluteDirectory(const String& path, const String& fallback, String& canonicalPath) {
  return canonicalizeAbsolutePath(path, fallback, canonicalPath);
}

bool canonicalizeAbsoluteDirectory(
    const SYSTEM::PsramString& path,
    const SYSTEM::PsramString& fallback,
    SYSTEM::PsramString& canonicalPath
) {
  return canonicalizeAbsolutePath(path, fallback, canonicalPath);
}

bool canonicalizeAbsoluteDirectory(const char* path, const char* fallback, SYSTEM::PsramString& canonicalPath) {
  return canonicalizeAbsolutePath(path, fallback, canonicalPath);
}

String ensureAbsoluteDirectory(const String& path, const String& fallback) {
  PsramString canonicalPath = ensureAbsoluteDirectory(path.c_str(), fallback.c_str());
  if (canonicalPath.empty()) {
    return String();
  }
  return toArduinoString(canonicalPath);
}

SYSTEM::PsramString ensureAbsoluteDirectory(
    const SYSTEM::PsramString& path,
    const SYSTEM::PsramString& fallback
) {
  PsramString canonicalPath;
  if (!canonicalizeAbsoluteDirectory(path, fallback, canonicalPath)) {
    return PsramString();
  }
  return canonicalPath;
}

SYSTEM::PsramString ensureAbsoluteDirectory(const char* path, const char* fallback) {
  PsramString canonicalPath;
  if (!canonicalizeAbsoluteDirectory(path, fallback, canonicalPath)) {
    return PsramString();
  }
  return canonicalPath;
}

String joinPaths(const String& baseDirectory, const String& childName) {
  const PsramString joined = joinPaths(baseDirectory.c_str(), childName.c_str());
  if (joined.empty()) {
    return String();
  }
  return toArduinoString(joined);
}

SYSTEM::PsramString joinPaths(
    const SYSTEM::PsramString& baseDirectory,
    const SYSTEM::PsramString& childName
) {
  return joinPathsImpl(makeView(baseDirectory), makeView(childName));
}

SYSTEM::PsramString joinPaths(const char* baseDirectory, const char* childName) {
  return joinPathsImpl(makeView(baseDirectory), makeView(childName));
}

String buildFullPath(const String& baseDirectory, const String& relativePath) {
  return joinPaths(baseDirectory, relativePath);
}

SYSTEM::PsramString buildFullPath(
    const SYSTEM::PsramString& baseDirectory,
    const SYSTEM::PsramString& relativePath
) {
  return joinPaths(baseDirectory, relativePath);
}

SYSTEM::PsramString buildFullPath(const char* baseDirectory, const char* relativePath) {
  return joinPaths(baseDirectory, relativePath);
}

bool isAccessAllowed(const String& canonicalNativePath, const FileManagerPathAccess access) {
  return isAccessAllowed(canonicalNativePath.c_str(), access);
}

bool isAccessAllowed(const SYSTEM::PsramString& canonicalNativePath, const FileManagerPathAccess access) {
  return isAccessAllowedImpl(makeView(canonicalNativePath), access);
}

bool isAccessAllowed(const char* canonicalNativePath, const FileManagerPathAccess access) {
  return isAccessAllowedImpl(makeView(canonicalNativePath), access);
}

}  // namespace FileManagerPathUtils
}  // namespace FILEMGR
