#pragma once

#include <cstddef>
#include <cstring>

namespace API::FILEMANAGER {

inline bool isLogStoragePath(const char* nativePath) {
    return nativePath && strncmp(nativePath, "/data/", 6) == 0;
}

// Treat "/data", "/data/<month>" and the active daily file as one protected
// subtree. The extra boundary check prevents false positives for prefix-only
// siblings such as "/data/2026-04x".
inline bool isSamePathOrParentDirectory(const char* candidatePath, const char* childPath) {
    if (!candidatePath || !childPath || candidatePath[0] == '\0' || childPath[0] == '\0') {
        return false;
    }

    const size_t candidateLen = strlen(candidatePath);
    if (strncmp(candidatePath, childPath, candidateLen) != 0) {
        return false;
    }

    return childPath[candidateLen] == '\0' || childPath[candidateLen] == '/';
}

inline bool shouldProtectActiveLogPath(const char* candidatePath, const char* activeLogPath) {
    return activeLogPath && activeLogPath[0] != '\0' &&
           isSamePathOrParentDirectory(candidatePath, activeLogPath);
}

// Large log downloads hold a read lease so logger rotation/delete code sees an
// active reader. Small inline downloads skip the lease on purpose to avoid
// paying the extra coordination cost for short-lived responses.
inline bool shouldAcquireLogReadLease(const char* nativePath,
                                      size_t fileSize,
                                      size_t inlineMaxBytes) {
    return fileSize > inlineMaxBytes && isLogStoragePath(nativePath);
}

}  // namespace API::FILEMANAGER
