#pragma once

#include <WString.h>
#include <cstring>

namespace API {

class MimeTypes {
 public:
  static const char* getContentType(const char* path) {
    if (!path) return "text/plain";
    if (endsWith(path, ".html") || endsWith(path, ".htm")) return "text/html";
    if (endsWith(path, ".css")) return "text/css";
    if (endsWith(path, ".json")) return "application/json";
    if (endsWith(path, ".js")) return "application/javascript";
    if (endsWith(path, ".png")) return "image/png";
    if (endsWith(path, ".gif")) return "image/gif";
    if (endsWith(path, ".jpg") || endsWith(path, ".jpeg")) return "image/jpeg";
    if (endsWith(path, ".ico")) return "image/x-icon";
    if (endsWith(path, ".svg")) return "image/svg+xml";
    if (endsWith(path, ".pdf")) return "application/pdf";
    if (endsWith(path, ".zip")) return "application/zip";
    if (endsWith(path, ".gz")) return "application/x-gzip";
    return "text/plain";
  }

  static const char* getContentType(const String& path) {
    return getContentType(path.c_str());
  }

 private:
  static bool endsWith(const char* path, const char* suffix) {
    if (!path || !suffix) return false;
    const size_t pathLen = strlen(path);
    const size_t suffixLen = strlen(suffix);
    if (suffixLen > pathLen) return false;
    return strcmp(path + (pathLen - suffixLen), suffix) == 0;
  }
};

} // namespace API
