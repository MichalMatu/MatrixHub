/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <security/SecuritySettingsService.h>
#include "../../src/system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "Security"

SecuritySettingsService::SecuritySettingsService(PsychicHttpServer *server, FS *fs) : _server(server),
                                                                                      _httpEndpoint(SecuritySettings::readForApi, SecuritySettings::update, this, server, SECURITY_SETTINGS_PATH, this),
                                                                                      _fsPersistence(SecuritySettings::readForStorage, SecuritySettings::update, this, fs, SECURITY_SETTINGS_FILE),
                                                                                      _jwtTokenService(FACTORY_JWT_SECRET),
                                                                                      _requestAuthorizer(&_rateLimiter, [this](const char* jwt) {
                                                                                          return this->authenticateJWT(jwt);
                                                                                      })
{
    addUpdateHandler([&](std::string_view originId) {
        (void)originId;
        configureJWTHandler();
        return StateHandlerResult::success();
    }, false);
}

void SecuritySettingsService::begin()
{
    _httpEndpoint.begin();
    // DEV-ONLY SECURITY EXCEPTION:
    // Startup remains permissive when the device is still on development
    // bootstrap credentials or freshly generated defaults. We intentionally do
    // not fail closed or raise a hard startup blocker yet because fast bring-up
    // and recovery matter more in the current dev phase. Production hardening
    // must revisit this.
    _fsPersistence.readFromFS();
    if (_state.pendingCredentialMigration)
    {
        _state.pendingCredentialMigration = false;
        if (!_fsPersistence.writeToFS())
        {
            LOGW("Failed to persist migrated password hashes to %s", SECURITY_SETTINGS_FILE);
        }
    }
    configureJWTHandler();
}

bool SecuritySettingsService::isSecurityEnabled()
{
    bool enabled = true;
    const StateHandlerResult readResult = read([&enabled](SecuritySettings& state) {
        enabled = !state.users.empty() && !state.jwtSecret.isEmpty();
    });

    if (!readResult.ok) {
        return true;
    }

    return enabled;
}

Authentication SecuritySettingsService::authenticateRequest(PsychicRequest *request)
{
    return _requestAuthorizer.authenticateRequest(request);
}

void SecuritySettingsService::configureJWTHandler()
{
    _jwtTokenService.configureSecret(_state.jwtSecret);
}

Authentication SecuritySettingsService::authenticateJWT(const char* jwt)
{
    Authentication result;
    (void)read([this, jwt, &result](SecuritySettings& state) {
        result = _jwtTokenService.authenticateJWT(jwt, state.users);
    });
    return result;
}

Authentication SecuritySettingsService::authenticate(const String &username, const String &password)
{
    Authentication result;
    (void)read([this, &username, &password, &result](SecuritySettings& state) {
        result = _jwtTokenService.authenticateCredentials(username, password, state.users);
    });
    return result;
}

String SecuritySettingsService::generateJWT(User *user)
{
    return _jwtTokenService.generateJWT(user);
}

PsychicRequestFilterFunction SecuritySettingsService::filterRequest(AuthenticationPredicate predicate)
{
    return _requestAuthorizer.filterRequest(predicate);
}

PsychicHttpRequestCallback SecuritySettingsService::wrapRequest(PsychicHttpRequestCallback onRequest, AuthenticationPredicate predicate)
{
    return _requestAuthorizer.wrapRequest(onRequest, predicate);
}

PsychicJsonRequestCallback SecuritySettingsService::wrapCallback(PsychicJsonRequestCallback onRequest, AuthenticationPredicate predicate)
{
    return _requestAuthorizer.wrapCallback(onRequest, predicate);
}

bool SecuritySettingsService::authorizeRequest(PsychicRequest *request,
                                               AuthenticationPredicate predicate,
                                               bool loadParams)
{
    return _requestAuthorizer.authorizeRequest(request, predicate, loadParams);
}

bool SecuritySettingsService::checkLoginRateLimit(PsychicRequest *request)
{
    if (!request || !request->client()) {
        return false;
    }
    return _loginRateLimiter.shouldAllow((uint32_t)request->client()->remoteIP());
}
