#pragma once

#include <Arduino.h>

#ifdef ESP32SVELTEKIT_USE_REAL_IMPL

#include <atomic>
#include <functional>

#include "../ESPmDNS.h"
#include "../FS.h"
#include "../LittleFS.h"
#include "../PsychicHttp.h"
#include "../PsychicHttpsServer.h"
#include "../WiFi.h"
#include "../security/SecurityManager.h"

#ifndef APP_NAME
#define APP_NAME "MatrixHub"
#endif

#ifndef APP_VERSION
#define APP_VERSION "2.0.0"
#endif

#ifndef ESPFS
#define ESPFS LittleFS
#endif

class APSettingsService;

class SecuritySettingsService : public SecurityManager {
public:
    struct RateLimiter {
        bool allow = true;

        bool shouldAllow(const IPAddress& ip) {
            (void)ip;
            return allow;
        }
    };

    SecuritySettingsService(PsychicHttpServer* server, FS* fs) {
        (void)server;
        (void)fs;
    }

    void begin() {}

    RateLimiter& getRateLimiter() {
        return _rateLimiter;
    }

private:
    RateLimiter _rateLimiter;
};

class WiFiSettingsService {
public:
    WiFiSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager) {
        (void)server;
        (void)fs;
        (void)securityManager;
    }

    void setAPSettingsService(APSettingsService* service) {
        _apSettingsService = service;
    }

    void setHostnameChangeCallback(std::function<void()> cb) {
        _onHostnameChangeCallback = cb;
    }

    void initWiFi() {}
    void begin() {}
    void loop() {}

    String getHostname() {
        return hostname;
    }

    void notifyHostnameChanged() {
        if (_onHostnameChangeCallback) {
            _onHostnameChangeCallback();
        }
    }

    String hostname = "device";

private:
    APSettingsService* _apSettingsService = nullptr;
    std::function<void()> _onHostnameChangeCallback = nullptr;
};

class WiFiScanner {
public:
    WiFiScanner(PsychicHttpServer* server, SecurityManager* securityManager) {
        (void)server;
        (void)securityManager;
    }

    void begin() {}
};

class WiFiStatus {
public:
    WiFiStatus(PsychicHttpServer* server, SecurityManager* securityManager) {
        (void)server;
        (void)securityManager;
    }

    void begin() {}
};

class APSettingsService {
public:
    APSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager) {
        (void)server;
        (void)fs;
        (void)securityManager;
    }

    void begin() {}
    void loop() {}
};

class APStatus {
public:
    APStatus(PsychicHttpServer* server, SecurityManager* securityManager, APSettingsService* apSettings) {
        (void)server;
        (void)securityManager;
        (void)apSettings;
    }

    void begin() {}
};

class NTPSettingsService {
public:
    NTPSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager) {
        (void)server;
        (void)fs;
        (void)securityManager;
    }

    void begin() {}
};

class NTPStatus {
public:
    NTPStatus(PsychicHttpServer* server, SecurityManager* securityManager) {
        (void)server;
        (void)securityManager;
    }

    void begin() {}
};

class AuthenticationService {
public:
    AuthenticationService(PsychicHttpServer* server, SecurityManager* securityManager) {
        (void)server;
        (void)securityManager;
    }

    void begin() {}
};

class SleepService {
public:
    SleepService(PsychicHttpServer* server, SecurityManager* securityManager) {
        (void)server;
        (void)securityManager;
    }

    void begin() {}
};

class RestartService {
public:
    RestartService(PsychicHttpServer* server, SecurityManager* securityManager) {
        (void)server;
        (void)securityManager;
    }

    void begin() {}
    static void scheduleRestart(uint32_t delayMs = 1500) { (void)delayMs; }
    static void restartNow() {}
    static bool isRestartPending() { return false; }
};

class FactoryResetService {
public:
    FactoryResetService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager) {
        (void)server;
        (void)fs;
        (void)securityManager;
    }

    void begin() {}
    void factoryReset() {}
};

class FeaturesService {
public:
    explicit FeaturesService(PsychicHttpServer* server) {
        (void)server;
    }

    void begin() {}
};

class ESP32SvelteKit {
public:
    ESP32SvelteKit(PsychicHttpServer* server = nullptr, unsigned int numberEndpoints = 45);

    void begin(bool mountFS = true);
    void begin(const char* cert, const char* key, bool mountFS = true);
    void loop();

    FS* getFS() {
        return &ESPFS;
    }

    PsychicHttpServer* getServer() {
        return _server;
    }

    SecurityManager* getSecurityManager() {
        return &_securitySettingsService;
    }

    SecuritySettingsService* getSecuritySettingsService() {
        return &_securitySettingsService;
    }

    WiFiSettingsService* getWiFiSettingsService() {
        return &_wifiSettingsService;
    }

    APSettingsService* getAPSettingsService() {
        return &_apSettingsService;
    }

    RestartService* getRestartService() {
        return &_restartService;
    }

    void factoryReset() {
        _factoryResetService.factoryReset();
    }

    void setMDNSAppName(String name) {
        _appName = name;
    }

private:
    void registerMDNSEventHandlers();
    void requestMDNSSync(bool bypassRetry = false);
    void syncMDNS();
    bool hasMDNSNetworkInterface() const;

    PsychicHttpServer* _server;
    unsigned int _numberEndpoints;

    SecuritySettingsService _securitySettingsService;
    WiFiSettingsService _wifiSettingsService;
    WiFiScanner _wifiScanner;
    WiFiStatus _wifiStatus;
    APSettingsService _apSettingsService;
    APStatus _apStatus;

    NTPSettingsService _ntpSettingsService;
    NTPStatus _ntpStatus;
    AuthenticationService _authenticationService;
    SleepService _sleepService;
    RestartService _restartService;
    FactoryResetService _factoryResetService;
    FeaturesService _featureService;

    String _appName = APP_NAME;
    const char* _ssl_cert = nullptr;
    const char* _ssl_key = nullptr;
    std::atomic<bool> _mdnsSyncPending{false};
    std::atomic<bool> _mdnsBypassRetryPending{false};
    bool _mdnsStarted = false;
    bool _hasMdnsSyncAttempt = false;
    uint32_t _lastMdnsSyncAttemptMs = 0;
};

#else

#include "../FS.h"
#include "../PsychicHttpServer.h"
#include "../security/SecurityManager.h"

class WiFiSettingsService {};

class ESP32SvelteKit {
public:
    explicit ESP32SvelteKit(PsychicHttpServer* server = nullptr, unsigned int numberEndpoints = 45)
        : _server(server), _numberEndpoints(numberEndpoints) {}

    void begin(bool mountFS = true) {
        beginCalls++;
        lastMountFs = mountFS;
        lastCert = nullptr;
        lastKey = nullptr;
    }

    void begin(const char* cert, const char* key, bool mountFS = true) {
        beginCalls++;
        lastCert = cert;
        lastKey = key;
        lastMountFs = mountFS;
    }

    void loop() {
        loopCalls++;
    }

    FS* getFS() {
        return &_fs;
    }

    PsychicHttpServer* getServer() {
        return _server;
    }

    SecurityManager* getSecurityManager() {
        return &_securityManager;
    }

    WiFiSettingsService* getWiFiSettingsService() {
        return &_wifiSettingsService;
    }

    PsychicHttpServer* _server = nullptr;
    unsigned int _numberEndpoints = 45;
    int beginCalls = 0;
    int loopCalls = 0;
    bool lastMountFs = true;
    const char* lastCert = nullptr;
    const char* lastKey = nullptr;

private:
    FS _fs;
    SecurityManager _securityManager;
    WiFiSettingsService _wifiSettingsService;
};

#endif
