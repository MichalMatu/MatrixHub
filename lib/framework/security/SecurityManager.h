#ifndef SecurityManager_h
#define SecurityManager_h

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


#include <security/ArduinoJsonJWT.h>
#include <PsychicHttp.h>
#include <list>
#include <memory>

#define SVK_TAG "🐼"

#define ACCESS_TOKEN_COOKIE "access_token"

#define AUTHORIZATION_HEADER "Authorization"
#define AUTHORIZATION_HEADER_PREFIX "Bearer "
#define AUTHORIZATION_HEADER_PREFIX_LEN 7

class User
{
public:
    String username;
    // Stores either a password hash or a legacy plain-text credential during migration.
    String password;
    bool admin;
    time_t validAfter;

public:
    User(const String &username, const String &password, bool admin) 
        : username(username), password(password), admin(admin), validAfter(0)
    {
    }
};

class Authentication
{
public:
    std::shared_ptr<User> user;
    bool authenticated;

public:
    Authentication(const User &user) : user(std::make_shared<User>(user)), authenticated(true)
    {
    }
    Authentication() : user(nullptr), authenticated(false)
    {
    }
    // Rule of Zero: Default copy/move/destructors are correct for shared_ptr
};

typedef std::function<bool(Authentication &authentication)> AuthenticationPredicate;

class AuthenticationPredicates
{
public:
    /*
     * Backend access model:
     * - public: endpoint is not wrapped
     * - authenticated: any logged-in user (admin or non-admin)
     * - admin: authenticated user with admin=true
     *
     * There is no separate backend "guest" role. UI may choose to hide some
     * authenticated read-only areas from non-admin users, but authorization in
     * firmware is expressed only with IS_AUTHENTICATED and IS_ADMIN.
     */
    static bool NONE_REQUIRED(Authentication &authentication)
    {
        return true;
    };
    static bool IS_AUTHENTICATED(Authentication &authentication)
    {
        return authentication.authenticated;
    };
    static bool IS_ADMIN(Authentication &authentication)
    {
        return authentication.authenticated && authentication.user && authentication.user->admin;
    };
};

class SecurityManager
{
public:
    /*
     * Authenticate, returning the user if found
     */
    virtual Authentication authenticate(const String &username, const String &password) = 0;

    /*
     * Generate a JWT for the user provided
     */
    virtual String generateJWT(User *user) = 0;

    /*
     * Check the request header for the Authorization token
     */
    virtual Authentication authenticateRequest(PsychicRequest *request) = 0;

    /*
     * Authenticate using a raw JWT string
     */
    virtual Authentication authenticateJWT(const char* jwt) = 0;

    /**
     * Filter a request with the provided predicate, only returning true if the predicate matches.
     */
    virtual PsychicRequestFilterFunction filterRequest(AuthenticationPredicate predicate) = 0;

    /**
     * Wrap the provided request to provide validation against an AuthenticationPredicate.
     */
    virtual PsychicHttpRequestCallback wrapRequest(PsychicHttpRequestCallback onRequest, AuthenticationPredicate predicate) = 0;

    /**
     * Wrap the provided json request callback to provide validation against an AuthenticationPredicate.
     */
    virtual PsychicJsonRequestCallback wrapCallback(PsychicJsonRequestCallback onRequest, AuthenticationPredicate predicate) = 0;

    /**
     * Check if the client IP is allowed to attempt login.
     */
    virtual bool checkLoginRateLimit(PsychicRequest *request) = 0;
};

#endif // end SecurityManager_h
