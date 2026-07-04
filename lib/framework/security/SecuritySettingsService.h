#ifndef SecuritySettingsService_h
#define SecuritySettingsService_h

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

#include <security/JwtTokenService.h>
#include <security/RequestAuthorizer.h>
#include <security/SecurityManager.h>
#include <security/SecuritySettings.h>
#include <security/RateLimiter.h>
#include "../../src/system/logging/Logging.h"
#include <core/HttpEndpoint.h>
#include <core/FSPersistence.h>


#define SECURITY_SETTINGS_FILE "/config/securitySettings.json"
#define SECURITY_SETTINGS_PATH "/rest/securitySettings"

class SecuritySettingsService : public StatefulService<SecuritySettings>, public SecurityManager
{
public:
    SecuritySettingsService(PsychicHttpServer *server, FS *fs);

    // Global Rate Limiter: 1200 requests per 60 seconds (20 req/s to allow CSI polling)
    RateLimiter _rateLimiter = RateLimiter(1200, 60000);
    
    // Login Rate Limiter: 3 attempts per 60 seconds (brute-force protection)
    RateLimiter _loginRateLimiter = RateLimiter(3, 60000);

    RateLimiter& getRateLimiter() { return _rateLimiter; }
    RateLimiter& getLoginRateLimiter() { return _loginRateLimiter; }
    bool isSecurityEnabled();

    bool checkLoginRateLimit(PsychicRequest *request) override;

    void begin();

    // Functions to implement SecurityManager
    Authentication authenticate(const String &username, const String &password) override;
    Authentication authenticateRequest(PsychicRequest *request) override;
    Authentication authenticateJWT(const char* jwt) override;
    String generateJWT(User *user) override;

    PsychicRequestFilterFunction filterRequest(AuthenticationPredicate predicate) override;
    PsychicHttpRequestCallback wrapRequest(PsychicHttpRequestCallback onRequest, AuthenticationPredicate predicate) override;
    PsychicJsonRequestCallback wrapCallback(PsychicJsonRequestCallback onRequest, AuthenticationPredicate predicate) override;

private:
    PsychicHttpServer *_server;

    HttpEndpoint<SecuritySettings> _httpEndpoint;
    FSPersistence<SecuritySettings> _fsPersistence;
    JwtTokenService _jwtTokenService;
    RequestAuthorizer _requestAuthorizer;

    bool authorizeRequest(PsychicRequest *request, AuthenticationPredicate predicate, bool loadParams);

    void configureJWTHandler();
};

#endif // end SecuritySettingsService_h
