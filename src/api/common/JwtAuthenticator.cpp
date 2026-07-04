#include "JwtAuthenticator.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "JwtAuth"

namespace API {

namespace {

bool validateJwt(SecurityManager* securityManager,
                 AuthenticationPredicate predicate,
                 const char* token) {
    if (!securityManager || !token || token[0] == '\0') {
        return false;
    }

    Authentication auth = securityManager->authenticateJWT(token);

    if (!auth.authenticated) {
        LOGW("[%s] WS Auth Failed: Invalid Token", LOG_TAG);
        return false;
    }

    if (predicate && !predicate(auth)) {
        LOGW("[%s] WS Auth Failed: Predicate rejected user", LOG_TAG);
        return false;
    }

    LOGD("[%s] WS Auth Success: User=%s", LOG_TAG, auth.user->username.c_str());
    return true;
}

} // namespace

JwtAuthenticator::JwtAuthenticator(SecurityManager* securityManager, AuthenticationPredicate predicate)
    : _securityManager(securityManager),
      _predicate(predicate) {
}

bool JwtAuthenticator::authenticate(httpd_req_t *req) {
    // Critical protection: If security manager is missing, deny access
    if (!_securityManager) {
        LOGE("[%s] WS Auth CRITICAL: No SecurityManager provided! Access denied.", LOG_TAG);
        return false;
    }

    char* token = (char*)heap_caps_malloc(1024, MALLOC_CAP_SPIRAM);
    if (!token) {
        LOGE("[%s] WS Auth Failed: PSRAM malloc failed for token", LOG_TAG);
        return false;
    }
    memset(token, 0, 1024);

    size_t tokenLen = 1024;
    // Websocket auth is intentionally cookie-only now. We used to accept
    // access_token in the query string, but that duplicated auth paths and made
    // websocket URLs harder to reason about and test.
    const esp_err_t cookieErr = httpd_req_get_cookie_val(req, ACCESS_TOKEN_COOKIE, token, &tokenLen);
    if (cookieErr == ESP_OK) {
        const bool authenticated = validateJwt(_securityManager, _predicate, token);
        heap_caps_free(token);
        return authenticated;
    }

    if (cookieErr != ESP_ERR_NOT_FOUND) {
        LOGW("[%s] WS Auth Failed: Cookie parse error (%d)", LOG_TAG, cookieErr);
        heap_caps_free(token);
        return false;
    }

    LOGW("[%s] WS Auth Failed: Missing access_token cookie", LOG_TAG);
    heap_caps_free(token);
    return false;
}

} // namespace API
