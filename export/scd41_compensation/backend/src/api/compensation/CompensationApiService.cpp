#include "CompensationApiService.h"

#include "../../compensation/CompensationSettingsService.h"

namespace API {

CompensationApiService::CompensationApiService(PsychicHttpServer* server,
                                               SecurityManager* securityManager,
                                               POWER::PowerManager* powerManager,
                                               COMPENSATION::CompensationSettingsService* settings)
    : BaseApiService(server, securityManager, powerManager, "api/compensation"),
      _settings(settings) {
    // Registry owns persistent compensation config; API keeps only the HTTP
    // contract while save/apply stays near the runtime service lifecycle.
    if (_settings) {
        _configEndpoint = std::make_unique<HttpEndpoint<RTC::CompensationData>>(
            COMPENSATION::CompensationSettingsService::readState,
            COMPENSATION::CompensationSettingsService::updateState,
            _settings,
            _server,
            "/api/compensation",
            _securityManager,
            AuthenticationPredicates::IS_ADMIN,
            AuthenticationPredicates::IS_AUTHENTICATED,
            nullptr,
            [this]() {
                if (_powerManager) {
                    _powerManager->notifyActivity(_activityTag);
                }
            });
    }
}

void CompensationApiService::begin() {
    if (_configEndpoint) {
        _configEndpoint->begin();
    }
}

}  // namespace API
