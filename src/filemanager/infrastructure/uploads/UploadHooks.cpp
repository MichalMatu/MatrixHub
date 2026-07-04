#include "filemanager/infrastructure/uploads/UploadHooks.h"

#include "system/logging/Logging.h"
#undef LOG_TAG
#define LOG_TAG "FileMgr"

namespace FILEMGR {
namespace UploadHooks {

void handleFileUploaded(fs::FS* fs, const char* nativePath, size_t totalBytes)
{
  // Hook for post-upload processing. Currently unused.
  LOGI("Uploaded %u bytes to %s", (unsigned)totalBytes, nativePath ? nativePath : "");
}

}  // namespace UploadHooks
}  // namespace FILEMGR
