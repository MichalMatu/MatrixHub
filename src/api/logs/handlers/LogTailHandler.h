#pragma once

#include <PsychicHttpServer.h>

namespace API {
namespace Handlers {

/**
 * @brief Handler for live tail log streaming endpoint
 */
class LogTailHandler {
public:
    static esp_err_t handleTail(PsychicRequest* request);
    static esp_err_t handleClear(PsychicRequest* request);
};

} // namespace Handlers
} // namespace API
