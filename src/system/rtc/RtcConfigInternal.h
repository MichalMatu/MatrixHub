#pragma once

#include "RtcConfig.h"

#include <freertos/FreeRTOS.h>

namespace RTC::detail {

TickType_t configLockTimeoutTicks();
TickType_t sleepSnapshotLockTimeoutTicks();
ConfigStore& requireStore();
void refreshConfigIntegrity(ConfigStore& cfg);
bool restoreConfigFromBackup(ConfigStore& cfg);
bool saveConfigBackupSnapshot(const ConfigStore& cfg);
void invalidateConfigBackupSnapshot();

}  // namespace RTC::detail
