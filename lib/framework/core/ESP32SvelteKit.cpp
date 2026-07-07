/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <core/ESP32SvelteKit.h>
#include <sdkconfig.h>
#include <esp_err.h>
#include <esp_netif.h>
#include <mdns.h>
#include <PsychicHttpsServer.h>
#include "../../src/system/logging/Logging.h"
#include "../../src/system/health/network/HttpServerHealthTracker.h"

#undef LOG_TAG
#define LOG_TAG "Framework"

namespace {

constexpr const char* kCacheControlNoStore = "no-store, max-age=0";
constexpr const char* kCacheControlImmutable = "public, max-age=31536000, immutable";
constexpr const char* kCacheControlWeekly = "public, max-age=604800";
constexpr uint32_t kMdnsRetryIntervalMs = 1000;
constexpr const char* kMdnsStaIfKey = "WIFI_STA_DEF";
constexpr const char* kMdnsApIfKey = "WIFI_AP_DEF";

bool isStaticAssetPath(const String& path) {
    if (path.isEmpty()) {
        return false;
    }

    if (path.startsWith("/_app/") ||
        path.startsWith("/api/") ||
        path.startsWith("/rest/") ||
        path.startsWith("/ws/") ||
        path.startsWith("/config/")) {
        return true;
    }

    const int lastSlash = path.lastIndexOf('/');
    const int segmentStart = lastSlash >= 0 ? lastSlash + 1 : 0;
    return path.indexOf('.', segmentStart) >= 0;
}

bool shouldServeSpaIndex(PsychicRequest* request) {
    if (!request || request->method() != HTTP_GET) {
        return false;
    }

    const String path = request->path();
    if (isStaticAssetPath(path)) {
        return false;
    }

    const String accept = request->header("Accept");
    // Deep links should fall back to index.html for browser navigations and
    // neutral probes such as curl's default Accept: */*. Missing JS/CSS/assets
    // still return 404 via isStaticAssetPath() above, so browsers do not parse
    // HTML as a module.
    if (accept.isEmpty() || accept.indexOf("text/html") >= 0) {
        return true;
    }
    return accept.indexOf("*/*") >= 0 && accept.indexOf("application/json") < 0;
}

const char* resolveStaticCacheControl(const String& uri) {
    if (uri.startsWith("/_app/immutable/")) {
        return kCacheControlImmutable;
    }
    if (uri == "/favicon.png") {
        return kCacheControlWeekly;
    }
    return kCacheControlNoStore;
}

void activateMDNSOnNetif(const char* ifKey, const char* label) {
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey(ifKey);
    if (!netif) {
        LOGW("MDNS %s netif not found: ifKey=%s", label, ifKey);
        return;
    }

    const esp_err_t registerErr = mdns_register_netif(netif);
    if (registerErr != ESP_OK && registerErr != ESP_ERR_INVALID_STATE) {
        LOGW("MDNS %s register failed: ifKey=%s err=%s",
             label,
             ifKey,
             esp_err_to_name(registerErr));
    }

    const auto actions = static_cast<mdns_event_actions_t>(
        MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ANNOUNCE_IP4);
    const esp_err_t actionErr = mdns_netif_action(netif, actions);
    if (actionErr != ESP_OK) {
        LOGW("MDNS %s enable/announce failed: ifKey=%s err=%s",
             label,
             ifKey,
             esp_err_to_name(actionErr));
        return;
    }

    LOGI("MDNS %s active on %s", label, ifKey);
}

void addStaticSecurityHeaders(PsychicResponse& response) {
    response.addHeader("X-Frame-Options", "DENY");
    response.addHeader("X-Content-Type-Options", "nosniff");
    response.addHeader("X-XSS-Protection", "1; mode=block");
    response.addHeader("Strict-Transport-Security", "max-age=31536000");
}

#ifdef EMBED_WWW
esp_err_t sendEmbeddedAssetResponse(PsychicRequest* request, const WWWAsset* asset) {
    if (!request) {
        return ESP_FAIL;
    }
    if (!asset) {
        return request->reply(404);
    }

    PsychicResponse response(request);
    response.setCode(200);
    response.setContentType(asset->contentType);
    response.addHeader("Content-Encoding", "gzip");
    response.addHeader("Cache-Control", resolveStaticCacheControl(asset->uri));
    addStaticSecurityHeaders(response);
    response.setContent(asset->content, asset->len);
    return response.send();
}
#endif

esp_err_t sendIndexResponse(PsychicRequest* request) {
    PsychicFileResponse response(request, ESPFS, "/www/index.html", "text/html");
    response.addHeader("Cache-Control", kCacheControlNoStore);
    addStaticSecurityHeaders(response);
    return response.send();
}

}  // namespace

ESP32SvelteKit::ESP32SvelteKit(PsychicHttpServer *server, unsigned int numberEndpoints) : _server(server),
                                                                                          _numberEndpoints(numberEndpoints),

                                                                                          _securitySettingsService(server, &ESPFS),
                                                                                          _wifiSettingsService(server, &ESPFS, &_securitySettingsService),
                                                                                          _wifiScanner(server, &_securitySettingsService),
                                                                                          _wifiStatus(server, &_securitySettingsService),
                                                                                          _apSettingsService(server, &ESPFS, &_securitySettingsService),
                                                                                          _apStatus(server, &_securitySettingsService, &_apSettingsService),


                                                                                          _ntpSettingsService(server, &ESPFS, &_securitySettingsService),
                                                                                          _ntpStatus(server, &_securitySettingsService),
                                                                                          _authenticationService(server, &_securitySettingsService),
                                                                                          _sleepService(server, &_securitySettingsService),
                                                                                          _restartService(server, &_securitySettingsService),
                                                                                          _factoryResetService(server, &ESPFS, &_securitySettingsService),
                                                                                          _featureService(server)
{
}

void ESP32SvelteKit::begin(const char* cert, const char* key, bool mountFS)
{
    _ssl_cert = cert;
    _ssl_key = key;
    begin(mountFS);
}


void ESP32SvelteKit::begin(bool mountFS)
{
    PsychicHttpRequestCallback spaIndexHandler;

    if (mountFS) {
        LOGV("Loading settings from files system");
        ESPFS.begin(true);
    }

    #ifdef EMBED_WWW
        LOGI("SERVER MODE: EMBEDDED (PROGMEM)");
    #else
        LOGI("SERVER MODE: FILESYSTEM (LittleFS)");
    #endif

    // IMPORTANT: Initialize Status monitors BEFORE starting services that might trigger events
    // _wifiStatus.begin(); // Moved after listen() to ensure server handle is valid

    
    // IMPORTANT: initWiFi() may immediately switch to AP mode (manageSTA())
    // when there are no configured STA networks. Ensure the AP settings
    // service is wired before that happens, otherwise SoftAP won't start.
    _wifiSettingsService.setAPSettingsService(&_apSettingsService);
    _wifiSettingsService.setHostnameChangeCallback([this]() {
        requestMDNSSync(true);
    });
    _wifiSettingsService.initWiFi();

    // SvelteKit uses a lot of handlers, so we need to increase the max_uri_handlers
    // WWWData has 77 Endpoints, Framework has 27, and Lighstate Demo has 4
    _server->config.max_uri_handlers = _numberEndpoints;

    // Keep the runtime server aligned with the central network profile instead of
    // forcing a higher hardcoded socket count at framework startup.
    if (_server->config.max_open_sockets < NET::HTTP::MAX_OPEN_SOCKETS) {
        _server->config.max_open_sockets = NET::HTTP::MAX_OPEN_SOCKETS;
    }

    // Global connection limiter must be installed before listen() so the first
    // inbound sockets during boot do not bypass it.
    _server->onOpen([this](PsychicClient* client) {
        // Count each accepted transport socket exactly once here. WebSocket
        // upgrade handlers run later on the same socket and must not increment
        // the health tracker again, otherwise "active clients" drifts upward.
        SYSTEM::HEALTH::HttpServerHealthTracker::recordOpen();
        IPAddress clientIP = client->remoteIP();
        if (!_securitySettingsService.getRateLimiter().shouldAllow(clientIP)) {
            LOGW("Rate limit exceeded for IP: %s (closing connection)", clientIP.toString().c_str());
            client->close();
        }
    });

    if (_ssl_cert && _ssl_key) {
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
        // Cast to PsychicHttpsServer to access ssl_config
        PsychicHttpsServer* secureServer = (PsychicHttpsServer*)_server;
        
        // CRITICAL: For HTTPS, we must configure ssl_config.httpd, NOT config!
        // The default is only 20 handlers which is too few for this application.
        secureServer->ssl_config.httpd.max_uri_handlers = _numberEndpoints;
        
        // SSL buffers use PSRAM (via mbedtls custom allocator), so we can handle more sockets.
        // Respect the value set in InitSequence::configureNetwork (from System.h)
        secureServer->ssl_config.httpd.max_open_sockets = _server->config.max_open_sockets;
        
        // Timeouts configured centrally in InitSequence::configureNetwork()
        secureServer->listen(443, _ssl_cert, _ssl_key);
        _server->maxRequestBodySize = 16384; // 16KB global limit
        LOGI("Started HTTPS Server on port 443 (max_uri_handlers=%d)", _numberEndpoints);

#else
        LOGE("HTTPS certificates provided but CONFIG_ESP_HTTPS_SERVER_ENABLE not defined!");
        _server->listen(80);
#endif
    } else {
        _server->listen(80);
    }

#ifdef EMBED_WWW
    // Serve embedded static resources through one lookup path. Split SvelteKit
    // builds can contain hundreds of immutable chunks, so registering each file
    // as a Psychic endpoint would exhaust the server URI handler budget.
    ESP_LOGV(SVK_TAG, "Using PROGMEM static asset lookup");
    spaIndexHandler = [](PsychicRequest *request) {
        return sendEmbeddedAssetResponse(request, WWWData::indexAsset());
    };
#else
    // Serve static resources from /www/
    LOGV("Registering routes from FS /www/ static resources");

    auto immutableHandler = _server->serveStatic("/_app/immutable/", ESPFS, "/www/_app/immutable/");
    immutableHandler->addHeader("Cache-Control", kCacheControlImmutable);

    auto envHandler = _server->serveStatic("/_app/env.js", ESPFS, "/www/_app/env.js");
    envHandler->addHeader("Cache-Control", kCacheControlNoStore);

    auto versionHandler = _server->serveStatic("/_app/version.json", ESPFS, "/www/_app/version.json");
    versionHandler->addHeader("Cache-Control", kCacheControlNoStore);

    auto manifestHandler = _server->serveStatic("/manifest.json", ESPFS, "/www/manifest.json");
    manifestHandler->addHeader("Cache-Control", kCacheControlNoStore);

    auto faviconHandler = _server->serveStatic("/favicon.png", ESPFS, "/www/favicon.png");
    faviconHandler->addHeader("Cache-Control", kCacheControlWeekly);

    spaIndexHandler = [](PsychicRequest *request) {
        return sendIndexResponse(request);
    };

    _server->on("/index.html", HTTP_GET, spaIndexHandler);
#endif

    _server->onNotFound([spaIndexHandler](PsychicRequest *request)
                        {
#ifdef EMBED_WWW
        if (request && request->method() == HTTP_GET) {
            const WWWAsset* asset = WWWData::findAsset(request->path());
            if (asset) {
                return sendEmbeddedAssetResponse(request, asset);
            }
        }
#endif

        if (shouldServeSpaIndex(request) && spaIndexHandler) {
            return spaIndexHandler(request);
        }

        if (request && request->method() == HTTP_GET && isStaticAssetPath(request->path())) {
            LOGW("Static asset miss: %s", request->path().c_str());
        }
        return request->reply(404); });

    // Serve static resources from /config/ if set by platformio.ini
#if SERVE_CONFIG_FILES
    _server->serveStatic("/config/", ESPFS, "/config/");
#endif



    registerMDNSEventHandlers();
    requestMDNSSync();
    syncMDNS();

#ifdef SERIAL_INFO
    LOGI("Running Firmware Version: %s", APP_VERSION);
#endif

    // Start the services
    _apStatus.begin();

    _apSettingsService.begin();
    _factoryResetService.begin();

    _restartService.begin();
    _wifiSettingsService.begin();
    _wifiScanner.begin();
    _wifiStatus.begin(); // Moved back to correct location after listen()

    _ntpSettingsService.begin();
    _ntpStatus.begin();

    _authenticationService.begin();
    _securitySettingsService.begin();

    _sleepService.begin();
    _featureService.begin();

}

void ESP32SvelteKit::loop()
{
    _wifiSettingsService.loop(); // 30 seconds
    _apSettingsService.loop();   // 10 seconds
    syncMDNS();
}

void ESP32SvelteKit::registerMDNSEventHandlers() {
    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        requestMDNSSync();
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        requestMDNSSync();
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        requestMDNSSync();
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_START);

    WiFi.onEvent([this](WiFiEvent_t event, WiFiEventInfo_t info) {
        (void)event;
        (void)info;
        requestMDNSSync();
    }, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STOP);
}

void ESP32SvelteKit::requestMDNSSync(bool bypassRetry) {
    if (bypassRetry) {
        _mdnsBypassRetryPending.store(true, std::memory_order_release);
    }
    _mdnsSyncPending.store(true, std::memory_order_release);
}

bool ESP32SvelteKit::hasMDNSNetworkInterface() const {
    const WiFiMode_t mode = WiFi.getMode();
    const bool apActive = mode == WIFI_AP || mode == WIFI_AP_STA;
    return apActive || WiFi.isConnected();
}

void ESP32SvelteKit::syncMDNS() {
    if (!_mdnsSyncPending.load(std::memory_order_acquire)) {
        return;
    }

    const uint32_t nowMs = millis();
    const bool bypassRetry = _mdnsBypassRetryPending.exchange(false, std::memory_order_acq_rel);
    if (!bypassRetry &&
        _hasMdnsSyncAttempt &&
        static_cast<uint32_t>(nowMs - _lastMdnsSyncAttemptMs) < kMdnsRetryIntervalMs) {
        return;
    }
    _hasMdnsSyncAttempt = true;
    _lastMdnsSyncAttemptMs = nowMs;

    if (!hasMDNSNetworkInterface()) {
        if (_mdnsStarted) {
            LOGI("Stopping MDNS: no active AP/STA interface");
            MDNS.end();
            _mdnsStarted = false;
        }
        _mdnsSyncPending.store(false, std::memory_order_release);
        return;
    }

    const String hostname = _wifiSettingsService.getHostname();
    if (hostname.isEmpty()) {
        if (_mdnsStarted) {
            LOGI("Stopping MDNS: hostname is empty");
            MDNS.end();
            _mdnsStarted = false;
        }
        LOGW("Skipping MDNS sync: hostname is empty");
        _mdnsSyncPending.store(false, std::memory_order_release);
        return;
    }

    const bool secure = _ssl_cert && _ssl_key;
    const WiFiMode_t mode = WiFi.getMode();
    LOGI("Refreshing MDNS: host=%s mode=%d sta=%d secure=%d",
         hostname.c_str(),
         static_cast<int>(mode),
         WiFi.isConnected() ? 1 : 0,
         secure ? 1 : 0);

    if (_mdnsStarted) {
        MDNS.end();
        _mdnsStarted = false;
    }

    if (!MDNS.begin(hostname.c_str())) {
        LOGW("MDNS begin failed for host=%s; retrying", hostname.c_str());
        return;
    }

    MDNS.setInstanceName(_appName);
    if (secure) {
        MDNS.addService("https", "tcp", 443);
        MDNS.addServiceTxt("https", "tcp", "Firmware Version", APP_VERSION);
    } else {
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "Firmware Version", APP_VERSION);
    }

    if (WiFi.isConnected()) {
        activateMDNSOnNetif(kMdnsStaIfKey, "STA");
    }
    if (mode == WIFI_AP || mode == WIFI_AP_STA) {
        activateMDNSOnNetif(kMdnsApIfKey, "AP");
    }

    _mdnsStarted = true;
    _mdnsSyncPending.store(false, std::memory_order_release);
}
