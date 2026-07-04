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

#include <wifi/WiFiSettingsService.h>

#include <WiFi.h>
#include <IPAddress.h>

#include <ArduinoJson.h>

#include <cstring>

#include <services/RestartService.h>
#include <PsychicHttpsServer.h>
#include "BackoffCalculator.h"
#include "../../../src/system/health/wifi/WifiHealthTracker.h"
#include "../../../src/system/logging/Logging.h"
#include "../../../src/system/rtc/RtcConfig.h"

// Guard window after WiFi.begin() to avoid reconfiguring while the stack is busy.
static constexpr uint32_t WIFI_CONNECT_GUARD_MS = 5000;
static constexpr const char* WIFI_FAST_TAG = "WiFiFast";
static constexpr const char* WIFI_MODE_CHANGE_ORIGIN = "matrix_menu";

namespace {

struct ConnectedTransitionInfo {
    bool recovered = false;
    unsigned long offlineDurationMs = 0;
    bool ipChanged = false;
    IPAddress previousIp = INADDR_NONE;
    WiFiConnectivityState previousState = WiFiConnectivityState::StaConnecting;
};

void refreshFastReconnectCache() {
    if (!WiFi.isConnected()) {
        return;
    }

    const uint8_t* bssid = WiFi.BSSID();
    const uint8_t channel = WiFi.channel();
    if (!bssid || channel == 0) {
        return;
    }

    RTC::networkState.update(channel,
                             bssid,
                             static_cast<uint32_t>(WiFi.localIP()),
                             static_cast<uint32_t>(WiFi.gatewayIP()),
                             static_cast<uint32_t>(WiFi.subnetMask()),
                             RTC::calculateSsidCrc(WiFi.SSID().c_str()));
}

const char* defaultReason(const char* reason, const char* fallback) {
    return (reason && reason[0] != '\0') ? reason : fallback;
}

void copyReason(char (&target)[32], const char* reason) {
    strlcpy(target, reason ? reason : "", sizeof(target));
}

bool isConfiguredStaticIp(const wifi_settings_t& network) {
    return network.staticIPConfig &&
           IPUtils::isSet(network.localIP) &&
           IPUtils::isSet(network.gatewayIP) &&
           IPUtils::isSet(network.subnetMask);
}

bool isZeroIp(const IPAddress& ip) {
    return static_cast<uint32_t>(ip) == 0;
}

}  // namespace

const char* wifiOperatingModeName(WiFiOperatingMode mode)
{
    switch (mode) {
        case WiFiOperatingMode::Off:
            return "off";
        case WiFiOperatingMode::AccessPoint:
            return "ap";
        case WiFiOperatingMode::Station:
            return "sta";
        default:
            return "unknown";
    }
}

const char* wifiConnectivityStateName(WiFiConnectivityState state)
{
    switch (state) {
        case WiFiConnectivityState::Off:
            return "off";
        case WiFiConnectivityState::ApOnly:
            return "ap_only";
        case WiFiConnectivityState::StaConnecting:
            return "sta_connecting";
        case WiFiConnectivityState::StaConnected:
            return "sta_connected";
        case WiFiConnectivityState::StaBackoff:
            return "sta_backoff";
        default:
            return "unknown";
    }
}

WiFiSettingsService::WiFiSettingsService(PsychicHttpServer *server,
                                         FS *fs,
                                         SecurityManager *securityManager)
    : _server(server),
      _securityManager(securityManager),
      _httpEndpoint(WiFiSettings::read,
                    WiFiSettings::update,
                    this,
                    server,
                    WIFI_SETTINGS_SERVICE_PATH,
                    securityManager,
                    AuthenticationPredicates::IS_ADMIN),
      _fsPersistence(WiFiSettings::read, WiFiSettings::update, this, fs, WIFI_SETTINGS_FILE),
      _lastConnectionAttempt(0) {
    addUpdateHandler([this](std::string_view originId) {
        (void)originId;
        const String currentHostname = _state.hostname;
        if (currentHostname != _lastAppliedHostname) {
            _lastAppliedHostname = currentHostname;
            if (_onHostnameChangeCallback) {
                _onHostnameChangeCallback();
            }
        }
        ESP_LOGI(SVK_TAG, "WiFi settings changed. Scheduling restart...");
        delayedReconnect();
        return StateHandlerResult::success();
    }, false);
}

void WiFiSettingsService::setAPSettingsService(APSettingsService *apSettingsService)
{
    _apSettingsService = apSettingsService;
}

void WiFiSettingsService::initWiFi()
{
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    _fsPersistence.readFromFS();
    _lastAppliedHostname = _state.hostname;
    applyConfiguredMode();
}

void WiFiSettingsService::begin()
{
    _httpEndpoint.begin();
}

void WiFiSettingsService::delayedReconnect()
{
    RestartService::scheduleRestart();
}

void WiFiSettingsService::loop()
{
    if (_state.mode != WiFiOperatingMode::Station) {
        return;
    }

    const unsigned long currentMillis = millis();
    observeConnectivity(currentMillis);
    (void)processRecoveryRequest();

    const RuntimeSnapshot runtime = snapshotRuntimeState();
    unsigned long delay = WIFI_RECONNECTION_DELAY;
    if (runtime.retryBackoffActive) {
        delay = WIFI_UTILS::calculateBackoffDelay(runtime.backoffLevel,
                                                  WIFI_RETRY_BACKOFF_BASE_MS,
                                                  WIFI_RETRY_BACKOFF_MAX_MS);
    }

    if (!runtime.lastConnectionAttempt ||
        (unsigned long)(currentMillis - runtime.lastConnectionAttempt) >= delay)
    {
        portENTER_CRITICAL(&_runtimeStateLock);
        _lastConnectionAttempt = currentMillis;
        _retryBackoffActive = false;
        portEXIT_CRITICAL(&_runtimeStateLock);
        manageSTA();
    }
}

String WiFiSettingsService::getHostname()
{
    return _state.hostname;
}

String WiFiSettingsService::getIP()
{
    if (WiFi.isConnected())
    {
        return WiFi.localIP().toString();
    }
    if (_state.mode == WiFiOperatingMode::AccessPoint &&
        _apSettingsService &&
        _apSettingsService->isAccessPointStarted())
    {
        return WiFi.softAPIP().toString();
    }
    return "Not connected";
}

WiFiOperatingMode WiFiSettingsService::getConfiguredMode() const
{
    return _state.mode;
}

bool WiFiSettingsService::isApModeConfigured() const
{
    return _state.mode == WiFiOperatingMode::AccessPoint;
}

bool WiFiSettingsService::setModeAndRestart(WiFiOperatingMode mode)
{
    const StateTransactionResult result = updateAndPropagate(
        [mode](WiFiSettings& settings) {
            if (mode == WiFiOperatingMode::Station && settings.wifiSettings.empty()) {
                return StateUpdateResult::ERROR;
            }
            if (settings.mode == mode) {
                return StateUpdateResult::UNCHANGED;
            }
            settings.mode = mode;
            return StateUpdateResult::CHANGED;
        },
        WIFI_MODE_CHANGE_ORIGIN);

    return result.outcome != StateUpdateResult::ERROR;
}

bool WiFiSettingsService::hasConfiguredNetworks() const
{
    return !_state.wifiSettings.empty();
}

WiFiSettingsService::RuntimeSnapshot WiFiSettingsService::snapshotRuntimeState() const
{
    RuntimeSnapshot snapshot = {};

    portENTER_CRITICAL(&_runtimeStateLock);
    snapshot.currentNetworkIndex = _currentNetworkIndex;
    snapshot.connectionAttempts = _connectionAttempts;
    snapshot.backoffLevel = _backoffLevel;
    snapshot.connectivityState = _connectivityState;
    snapshot.retryCycleCompleted = _retryCycleCompleted;
    snapshot.retryBackoffActive = _retryBackoffActive;
    snapshot.recoveryRequested = _recoveryRequested;
    snapshot.lastConnectionAttempt = _lastConnectionAttempt;
    snapshot.connectingSince = _connectingSince;
    snapshot.disconnectedSince = _disconnectedSince;
    snapshot.stableConnectedSince = _stableConnectedSince;
    snapshot.lastIpChangeMs = _lastIpChangeMs;
    snapshot.lastKnownStaIp = _lastKnownStaIp;
    copyReason(snapshot.pendingRecoveryReason, _pendingRecoveryReason);
    copyReason(snapshot.lastRecoveryReason, _lastRecoveryReason);
    portEXIT_CRITICAL(&_runtimeStateLock);

    return snapshot;
}

bool WiFiSettingsService::requestRecovery(const char* reason)
{
    if (_state.mode != WiFiOperatingMode::Station) {
        ESP_LOGW(SVK_TAG, "Ignoring WiFi recovery request outside STA mode");
        return false;
    }

    if (!hasConfiguredNetworks()) {
        ESP_LOGW(SVK_TAG, "Ignoring WiFi recovery request: no configured STA networks");
        return false;
    }

    if (WiFi.isConnected()) {
        return true;
    }

    char activeReason[32]{0};
    bool coalesced = false;
    portENTER_CRITICAL(&_runtimeStateLock);
    if (_recoveryRequested) {
        copyReason(activeReason, _pendingRecoveryReason);
        coalesced = true;
    } else {
        copyReason(_pendingRecoveryReason, defaultReason(reason, "external"));
        copyReason(activeReason, _pendingRecoveryReason);
        _recoveryRequested = true;
    }
    portEXIT_CRITICAL(&_runtimeStateLock);

    if (coalesced) {
        ESP_LOGI(SVK_TAG, "Coalescing WiFi recovery request: pending=%s new=%s",
             activeReason,
             defaultReason(reason, "external"));
        return true;
    }

    ESP_LOGW(SVK_TAG, "Queued WiFi recovery request: reason=%s", activeReason);
    return true;
}

WiFiConnectivityDiagnostics WiFiSettingsService::getConnectivityDiagnostics() const
{
    WiFiConnectivityDiagnostics diagnostics = {};
    const RuntimeSnapshot runtime = snapshotRuntimeState();
    const bool staConnected = WiFi.isConnected();
    const IPAddress staIp = staConnected ? WiFi.localIP() : IPAddress(INADDR_NONE);
    const wifi_mode_t currentWifiMode = WiFi.getMode();
    const bool apActive = (_apSettingsService && _apSettingsService->isAccessPointStarted()) ||
                          currentWifiMode == WIFI_AP ||
                          currentWifiMode == WIFI_AP_STA;

    diagnostics.state = runtime.connectivityState;
    diagnostics.configuredMode = _state.mode;
    diagnostics.wifiMode = currentWifiMode;
    diagnostics.staConnected = staConnected;
    diagnostics.apActive = apActive;
    diagnostics.disconnectedSinceMs = runtime.disconnectedSince;
    diagnostics.stableConnectedSinceMs = runtime.stableConnectedSince;
    diagnostics.lastIpChangeMs = runtime.lastIpChangeMs;
    diagnostics.lastDisconnectReason = SYSTEM::HEALTH::WifiHealthTracker::getHealth().lastDisconnectReason;
    diagnostics.apStationCount = static_cast<uint8_t>(WiFi.softAPgetStationNum());
    diagnostics.apIp = WiFi.softAPIP();
    diagnostics.staIp = staIp;
    copyReason(diagnostics.lastRecoveryReason, runtime.lastRecoveryReason);

    const size_t diagnosticsNetworkIndex = WIFI_DIAGNOSTICS::selectNetworkIndex(
        _state.wifiSettings,
        diagnostics.staConnected ? WiFi.SSID().c_str() : nullptr,
        diagnostics.staConnected,
        runtime.currentNetworkIndex,
        [](const wifi_settings_t& network) {
            return network.ssid.c_str();
        });

    if (diagnosticsNetworkIndex != WIFI_DIAGNOSTICS::kNoNetworkIndex) {
        const wifi_settings_t& primary = _state.wifiSettings[diagnosticsNetworkIndex];
        diagnostics.savedStaticIpConfigured = isConfiguredStaticIp(primary);
        if (diagnostics.savedStaticIpConfigured) {
            diagnostics.savedStaticIp = primary.localIP;
            diagnostics.savedStaticIpMatches =
                diagnostics.staConnected && (primary.localIP == diagnostics.staIp);
        }
    }

    bool httpsRunning = false;
#ifdef CONFIG_ESP_HTTPS_SERVER_ENABLE
    if (_server && _server->server) {
        auto* secureServer = static_cast<PsychicHttpsServer*>(_server);
        httpsRunning = secureServer->ssl_config.port_secure == 443;
    }
#endif

    diagnostics.portForwardingReady =
        _state.mode == WiFiOperatingMode::Station &&
        diagnostics.staConnected &&
        diagnostics.savedStaticIpConfigured &&
        diagnostics.savedStaticIpMatches &&
        httpsRunning &&
        !diagnostics.apActive;

    return diagnostics;
}

void WiFiSettingsService::observeConnectivity(unsigned long nowMs)
{
    if (WiFi.isConnected()) {
        handleConnected(nowMs);
        return;
    }

    portENTER_CRITICAL(&_runtimeStateLock);
    if (_stableConnectedSince != 0) {
        _stableConnectedSince = 0;
    }

    if (_disconnectedSince == 0) {
        _disconnectedSince = nowMs;
    }

    _connectivityState = _retryBackoffActive
        ? WiFiConnectivityState::StaBackoff
        : WiFiConnectivityState::StaConnecting;
    portEXIT_CRITICAL(&_runtimeStateLock);
}

void WiFiSettingsService::handleConnected(unsigned long nowMs)
{
    const IPAddress currentIp = WiFi.localIP();
    ConnectedTransitionInfo transition = {};

    portENTER_CRITICAL(&_runtimeStateLock);
    transition.previousState = _connectivityState;
    if (_disconnectedSince != 0) {
        transition.recovered = true;
        transition.offlineDurationMs = static_cast<unsigned long>(nowMs - _disconnectedSince);
        _disconnectedSince = 0;
    }

    if (_stableConnectedSince == 0) {
        _stableConnectedSince = nowMs;
    }

    _connectingSince = 0;
    _currentNetworkIndex = 0;
    _connectionAttempts = 0;
    _retryBackoffActive = false;
    _retryCycleCompleted = false;
    _backoffLevel = 0;
    _recoveryRequested = false;
    _pendingRecoveryReason[0] = '\0';
    _connectivityState = WiFiConnectivityState::StaConnected;

    if (_lastKnownStaIp != currentIp) {
        transition.ipChanged = true;
        transition.previousIp = _lastKnownStaIp;
        _lastKnownStaIp = currentIp;
        _lastIpChangeMs = nowMs;
    }
    portEXIT_CRITICAL(&_runtimeStateLock);

    if (transition.recovered) {
        ESP_LOGI(SVK_TAG, "STA recovered after %lu ms offline (state=%s)",
             transition.offlineDurationMs,
             wifiConnectivityStateName(transition.previousState));
    }

    refreshFastReconnectCache();

    if (transition.ipChanged) {
        if (!isZeroIp(transition.previousIp)) {
            ESP_LOGI(SVK_TAG, "STA IP changed: %s -> %s",
                 transition.previousIp.toString().c_str(),
                 currentIp.toString().c_str());
        } else {
            ESP_LOGI(SVK_TAG, "STA IP acquired: %s", currentIp.toString().c_str());
        }
    }
}

bool WiFiSettingsService::processRecoveryRequest()
{
    const bool staConnected = WiFi.isConnected();
    char reason[32]{0};

    portENTER_CRITICAL(&_runtimeStateLock);
    if (!_recoveryRequested) {
        portEXIT_CRITICAL(&_runtimeStateLock);
        return false;
    }

    if (staConnected) {
        _recoveryRequested = false;
        _pendingRecoveryReason[0] = '\0';
        portEXIT_CRITICAL(&_runtimeStateLock);
        return false;
    }

    if (_state.mode != WiFiOperatingMode::Station || _state.wifiSettings.empty()) {
        copyReason(reason, _pendingRecoveryReason);
        _recoveryRequested = false;
        _pendingRecoveryReason[0] = '\0';
        portEXIT_CRITICAL(&_runtimeStateLock);
        ESP_LOGW(SVK_TAG, "Dropping WiFi recovery request outside valid STA mode: reason=%s", reason);
        return false;
    }

    copyReason(reason, _pendingRecoveryReason);
    copyReason(_lastRecoveryReason, _pendingRecoveryReason);

    _connectingSince = 0;
    _lastConnectionAttempt = 0;
    _currentNetworkIndex = 0;
    _connectionAttempts = 0;
    _retryBackoffActive = false;
    _retryCycleCompleted = false;
    _backoffLevel = 0;
    _recoveryRequested = false;
    _pendingRecoveryReason[0] = '\0';
    _connectivityState = WiFiConnectivityState::StaConnecting;
    portEXIT_CRITICAL(&_runtimeStateLock);

    ESP_LOGW(SVK_TAG, "Applying WiFi recovery in STA-only mode: reason=%s", reason);

    WiFi.disconnect(false);
    WiFi.mode(WIFI_STA);

    return true;
}

void WiFiSettingsService::applyConfiguredMode()
{
    switch (_state.mode) {
        case WiFiOperatingMode::Off:
            switchToOffMode("configured_off");
            return;

        case WiFiOperatingMode::AccessPoint:
            (void)switchToAPMode("configured_ap");
            return;

        case WiFiOperatingMode::Station:
            if (_state.wifiSettings.empty()) {
                ESP_LOGW(SVK_TAG, "Stored STA mode has no networks. Falling back to AP mode.");
                _state.mode = WiFiOperatingMode::AccessPoint;
                (void)switchToAPMode("sta_without_networks");
                return;
            }
            if (_apSettingsService) {
                _apSettingsService->stopAccessPoint();
            }
            WiFi.setHostname(_state.hostname.c_str());
            WiFi.mode(WIFI_STA);
            resetStaRuntime(WiFiConnectivityState::StaConnecting);
            manageSTA();
            return;
    }
}

void WiFiSettingsService::manageSTA()
{
    const RuntimeSnapshot runtime = snapshotRuntimeState();

    if (_state.mode != WiFiOperatingMode::Station)
    {
        return;
    }

    if (_state.wifiSettings.empty())
    {
        ESP_LOGW(SVK_TAG, "STA mode has no networks. Switching to AP mode.");
        _state.mode = WiFiOperatingMode::AccessPoint;
        (void)switchToAPMode("sta_without_networks");
        return;
    }

    if (WiFi.isConnected())
    {
        return;
    }

    if (runtime.connectingSince != 0 &&
        (unsigned long)(millis() - runtime.connectingSince) < WIFI_CONNECT_GUARD_MS)
    {
        return;
    }

    connectToWiFi();
}

void WiFiSettingsService::connectToWiFi()
{
    if (_state.wifiSettings.empty())
    {
        return;
    }

    const size_t networkCount = _state.wifiSettings.size();
    size_t networkIndex = 0;
    uint8_t attemptNumber = 0;

    portENTER_CRITICAL(&_runtimeStateLock);
    while (_currentNetworkIndex < networkCount && _connectionAttempts >= WIFI_MAX_ATTEMPTS_PER_NETWORK)
    {
        _connectionAttempts = 0;
        _currentNetworkIndex++;
    }

    if (_currentNetworkIndex >= networkCount)
    {
        unsigned long backoffDelay = WIFI_UTILS::calculateBackoffDelay(_backoffLevel,
                                                                       WIFI_RETRY_BACKOFF_BASE_MS,
                                                                       WIFI_RETRY_BACKOFF_MAX_MS);
        const uint8_t loggedBackoffLevel = _backoffLevel;

        _retryCycleCompleted = true;
        _currentNetworkIndex = 0;
        _connectionAttempts = 0;
        _lastConnectionAttempt = millis();
        _retryBackoffActive = true;
        _connectivityState = WiFiConnectivityState::StaBackoff;

        if (_backoffLevel < 4) {
            _backoffLevel++;
        }
        portEXIT_CRITICAL(&_runtimeStateLock);

        ESP_LOGW(SVK_TAG, "All STA networks failed. Retrying in %u seconds... (level %u)",
                 static_cast<unsigned>(backoffDelay / 1000),
                 loggedBackoffLevel);

        return;
    }

    networkIndex = _currentNetworkIndex;
    _connectionAttempts++;
    attemptNumber = _connectionAttempts;
    _connectivityState = WiFiConnectivityState::StaConnecting;
    portEXIT_CRITICAL(&_runtimeStateLock);

    wifi_settings_t &network = _state.wifiSettings[networkIndex];

    ESP_LOGD(SVK_TAG,
             "Connecting to network: %s (attempt %u/%u)",
             network.ssid.c_str(),
             attemptNumber,
             WIFI_MAX_ATTEMPTS_PER_NETWORK);

    if (_onActivityCallback) {
        _onActivityCallback();
    }

    configureNetwork(network);
    portENTER_CRITICAL(&_runtimeStateLock);
    _connectingSince = millis();

    if (_connectionAttempts >= WIFI_MAX_ATTEMPTS_PER_NETWORK)
    {
        _connectionAttempts = WIFI_MAX_ATTEMPTS_PER_NETWORK;
    }
    portEXIT_CRITICAL(&_runtimeStateLock);
}

void WiFiSettingsService::configureNetwork(wifi_settings_t &network)
{
    if (network.staticIPConfig)
    {
        WiFi.config(network.localIP, network.gatewayIP, network.subnetMask, network.dnsIP1, network.dnsIP2);
    }
    else
    {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    WiFi.setHostname(_state.hostname.c_str());
    WiFi.mode(WIFI_STA);

    const uint32_t targetSsidCrc = RTC::calculateSsidCrc(network.ssid.c_str());

    if (RTC::networkState.matchesSsid(targetSsidCrc)) {
        LOG::Logging::log(ESP_LOG_INFO, WIFI_FAST_TAG,
                          "Fast-connect using cached BSSID (ch=%u)",
                          RTC::networkState.channel);
        WiFi.begin(network.ssid.c_str(),
                   network.password.c_str(),
                   RTC::networkState.channel,
                   RTC::networkState.bssid,
                   true);
        RTC::networkState.invalidate();
    } else {
        WiFi.begin(network.ssid.c_str(), network.password.c_str());
    }

    WiFi.setTxPower(WIFI_POWER_15dBm);
    WiFi.setSleep(false);
}

bool WiFiSettingsService::switchToAPMode(const char* reason)
{
    ESP_LOGI(SVK_TAG, "Switching to AP-only mode: reason=%s", defaultReason(reason, "configured_ap"));
    WiFi.setHostname(_state.hostname.c_str());

    if (!_apSettingsService || !_apSettingsService->startAccessPoint())
    {
        ESP_LOGE(SVK_TAG, "AP-only transition aborted: SoftAP failed to start");
        return false;
    }

    resetStaRuntime(WiFiConnectivityState::ApOnly);
    return true;
}

void WiFiSettingsService::switchToOffMode(const char* reason)
{
    ESP_LOGI(SVK_TAG, "Switching WiFi off: reason=%s", defaultReason(reason, "configured_off"));
    if (_apSettingsService) {
        _apSettingsService->stopAccessPoint();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    resetStaRuntime(WiFiConnectivityState::Off);
}

void WiFiSettingsService::resetStaRuntime(WiFiConnectivityState state)
{
    portENTER_CRITICAL(&_runtimeStateLock);
    _currentNetworkIndex = 0;
    _connectionAttempts = 0;
    _backoffLevel = 0;
    _connectivityState = state;
    _retryCycleCompleted = false;
    _retryBackoffActive = false;
    _recoveryRequested = false;
    _lastConnectionAttempt = 0;
    _connectingSince = 0;
    _disconnectedSince = 0;
    _stableConnectedSince = 0;
    _pendingRecoveryReason[0] = '\0';
    portEXIT_CRITICAL(&_runtimeStateLock);
}
