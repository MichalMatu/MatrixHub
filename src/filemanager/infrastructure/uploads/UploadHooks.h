#pragma once

#include <FS.h>
#include <Arduino.h>

namespace FILEMGR {
namespace UploadHooks {

/**
 * @brief Handle logic executed after a file upload completes.
 *
 * @param fs Filesystem instance where the file resides.
 * @param nativePath Absolute path (native to the filesystem) of the uploaded file.
 * @param totalBytes Size of the uploaded payload in bytes.
 */
void handleFileUploaded(fs::FS* fs, const char* nativePath, size_t totalBytes);

}  // namespace UploadHooks
}  // namespace FILEMGR
