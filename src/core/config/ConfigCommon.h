#pragma once

#include <FS.h>
#include <freertos/semphr.h>

extern SemaphoreHandle_t g_fsMutex;

namespace CONFIG {

constexpr size_t kConfigDocSize = 12288;
constexpr size_t kConfigFileMaxBytes = 16 * 1024;
constexpr const char* kConfigFile = "/config/config.json";
constexpr const char* kConfigTmpFile = "/config/settings.tmp";
constexpr const char* kConfigBakFile = "/config/settings.bak";

}  // namespace CONFIG
