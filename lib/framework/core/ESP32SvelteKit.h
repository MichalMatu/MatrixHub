#ifndef ESP32SvelteKit_h
#define ESP32SvelteKit_h

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

#include <Arduino.h>
#include <atomic>

#include <WiFi.h>
#include <ESPmDNS.h>

#include <wifi/APSettingsService.h>
#include <wifi/APStatus.h>
#include <security/AuthenticationService.h>

#include <services/FactoryResetService.h>
#include <services/FeaturesService.h>

#include <network/NTPSettingsService.h>
#include <network/NTPStatus.h>
#include <services/RestartService.h>
#include <security/SecuritySettingsService.h>
#include <services/SleepService.h>

#include <wifi/WiFiScanner.h>
#include <wifi/WiFiSettingsService.h>
#include <wifi/WiFiStatus.h>
#include <core/ESPFS.h>
#include <PsychicHttp.h>


#ifdef EMBED_WWW
#include <core/WWWData.h>
#endif





// define callback function to include into the main loop


// enum for connection status


class ESP32SvelteKit
{
public:
    ESP32SvelteKit(PsychicHttpServer *server, unsigned int numberEndpoints = 45);

    void begin(bool mountFS = true);
    
    // HTTPS Support
    void begin(const char* cert, const char* key, bool mountFS = true);


    void loop();



    FS *getFS()
    {
        return &ESPFS;
    }

    PsychicHttpServer *getServer()
    {
        return _server;
    }

    SecurityManager *getSecurityManager()
    {
        return &_securitySettingsService;
    }

    SecuritySettingsService *getSecuritySettingsService()
    {
        return &_securitySettingsService;
    }

    WiFiSettingsService *getWiFiSettingsService()
    {
        return &_wifiSettingsService;
    }

    APSettingsService *getAPSettingsService()
    {
        return &_apSettingsService;
    }



    RestartService *getRestartService()
    {
        return &_restartService;
    }

    void factoryReset()
    {
        _factoryResetService.factoryReset();
    }

    NTPSettingsService *getNTPSettingsService()
    {
        return &_ntpSettingsService;
    }

    SleepService *getSleepService()
    {
        return &_sleepService;
    }

    void setMDNSAppName(String name)
    {
        _appName = name;
    }

private:
    void registerMDNSEventHandlers();
    void requestMDNSSync(bool bypassRetry = false);
    void syncMDNS();
    bool hasMDNSNetworkInterface() const;

    PsychicHttpServer *_server;

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


protected:





};

#endif
