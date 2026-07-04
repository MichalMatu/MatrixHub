#ifndef SecurityManager_h
#define SecurityManager_h

#include <Arduino.h>
#include <ArduinoJson.h>

#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <esp_http_server.h>

class PsychicRequest;

#ifndef ACCESS_TOKEN_COOKIE
#define ACCESS_TOKEN_COOKIE "access_token"
#endif

#ifndef AUTHORIZATION_HEADER
#define AUTHORIZATION_HEADER "Authorization"
#endif

#ifndef AUTHORIZATION_HEADER_PREFIX
#define AUTHORIZATION_HEADER_PREFIX "Bearer "
#endif

#ifndef AUTHORIZATION_HEADER_PREFIX_LEN
#define AUTHORIZATION_HEADER_PREFIX_LEN 7
#endif

struct User {
    String username;
    String password;
    bool admin = true;
    time_t validAfter = 0;

    User() = default;

    User(const String& usernameValue, const String& passwordValue, bool adminValue)
        : username(usernameValue), password(passwordValue), admin(adminValue), validAfter(0) {}
};

struct Authentication {
    std::shared_ptr<User> user = nullptr;
    bool authenticated = false;
    bool admin = false;

    Authentication() = default;

    explicit Authentication(const User& userValue)
        : user(std::make_shared<User>(userValue)),
          authenticated(true),
          admin(userValue.admin) {}

    Authentication(bool authenticatedValue, bool adminValue, std::string username = "stub")
        : user(authenticatedValue
                   ? std::make_shared<User>(String(username.c_str()), String(), adminValue)
                   : nullptr),
          authenticated(authenticatedValue),
          admin(adminValue) {}
};

using AuthenticationPredicate = std::function<bool(Authentication&)>;
using PsychicHttpRequestCallback = std::function<esp_err_t(PsychicRequest*)>;
using PsychicJsonRequestCallback = std::function<esp_err_t(PsychicRequest*, JsonVariant&)>;

class AuthenticationPredicates {
public:
    static bool IS_AUTHENTICATED(Authentication &authentication) { return authentication.authenticated; }
    static bool IS_ADMIN(Authentication &authentication) {
        return authentication.authenticated &&
               ((authentication.user && authentication.user->admin) || authentication.admin);
    }
};

class SecurityManager {
public:
    virtual ~SecurityManager() = default;

    virtual Authentication authenticateJWT(const char* jwt) {
        (void)jwt;
        return Authentication{};
    }

    virtual PsychicHttpRequestCallback wrapRequest(PsychicHttpRequestCallback onRequest, AuthenticationPredicate) {
        return onRequest;
    }

    virtual PsychicJsonRequestCallback wrapCallback(PsychicJsonRequestCallback onRequest, AuthenticationPredicate) {
        return onRequest;
    }
};

#endif
