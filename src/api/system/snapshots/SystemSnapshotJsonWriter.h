#pragma once

#include <core/StatefulService.h>

#include "../../../system/utils/json/JsonResponseWriter.h"
#include "SystemApiSnapshots.h"

namespace API {

class SystemSnapshotJsonWriter {
public:
    static void writeSystemInfo(
        Utils::JsonResponseWriter& writer,
        const SystemInfoSnapshot& snapshot);
    static void writeTasks(
        Utils::JsonResponseWriter& writer,
        const SystemTasksSnapshot& snapshot);
    static StateHandlerResult writeNetwork(
        Utils::JsonResponseWriter& writer,
        const SystemNetworkSnapshot& snapshot);
};

}  // namespace API
