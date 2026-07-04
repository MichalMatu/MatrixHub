#include "LiveTailApiService.h"
#include "handlers/LogTailHandler.h"

namespace API {

LiveTailApiService::LiveTailApiService(PsychicHttpServer* server, SecurityManager* securityManager, POWER::PowerManager* powerManager)
    : BaseApiService(server, securityManager, powerManager, "rest/logs/tail") {}

void LiveTailApiService::begin() {
    _server->on("/rest/logs/tail", HTTP_GET,
                wrapAdmin([this](PsychicRequest* request) {
                    return Handlers::LogTailHandler::handleTail(request);
                }));

    _server->on("/rest/logs/tail", HTTP_DELETE,
                wrapAdmin([this](PsychicRequest* request) {
                    return Handlers::LogTailHandler::handleClear(request);
                }));
}

}  // namespace API
