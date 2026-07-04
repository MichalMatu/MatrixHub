#pragma once

#include "DiagnosticsSnapshots.h"
#include "../../system/utils/json/JsonResponseWriter.h"

namespace API {

class DiagnosticsJsonWriter {
public:
    static void writeHeap(Utils::JsonResponseWriter& writer, const DiagnosticsHeapSnapshot& snapshot);
    static void writeSummary(Utils::JsonResponseWriter& writer, const DiagnosticsSummarySnapshot& snapshot);
    static void writeFeatures(Utils::JsonResponseWriter& writer, const DiagnosticsFeaturesSnapshot& snapshot);
    static void writeMutexes(Utils::JsonResponseWriter& writer);
    static void writeEndpoints(Utils::JsonResponseWriter& writer);

private:
    static void writeHeapRegion(
        Utils::JsonResponseWriter& writer,
        const DiagnosticsHeapRegionSnapshot& region);
    static void writeHttpHealth(
        Utils::JsonResponseWriter& writer,
        const SYSTEM::HEALTH::HttpServerHealthSnapshot& snapshot);
};

}  // namespace API
