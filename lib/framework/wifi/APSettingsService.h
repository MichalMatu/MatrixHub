#ifndef APSettingsConfig_h
#define APSettingsConfig_h

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

#include <utils/SettingValue.h>
#include <core/HttpEndpoint.h>
#include <core/FSPersistence.h>
#include <utils/JsonUtils.h>
#include <string_view>

#include <IPAddress.h>

class DNSServer;

#ifndef FACTORY_AP_SSID
#define FACTORY_AP_SSID "matrixhub.local"
#endif

#ifndef FACTORY_AP_PASSWORD
// Default AP mode is open so factory reset and first-run setup remain reachable.
#define FACTORY_AP_PASSWORD ""
#endif

#ifndef FACTORY_AP_LOCAL_IP
#define FACTORY_AP_LOCAL_IP "192.168.4.1"
#endif

#ifndef FACTORY_AP_GATEWAY_IP
#define FACTORY_AP_GATEWAY_IP "192.168.4.1"
#endif

#ifndef FACTORY_AP_SUBNET_MASK
#define FACTORY_AP_SUBNET_MASK "255.255.255.0"
#endif

#ifndef FACTORY_AP_CHANNEL
#define FACTORY_AP_CHANNEL 1
#endif

#ifndef FACTORY_AP_SSID_HIDDEN
#define FACTORY_AP_SSID_HIDDEN false
#endif

#ifndef FACTORY_AP_MAX_CLIENTS
#define FACTORY_AP_MAX_CLIENTS 4
#endif

#define AP_SETTINGS_FILE "/config/apSettings.json"
#define AP_SETTINGS_SERVICE_PATH "/rest/apSettings"

#define MANAGE_NETWORK_DELAY 10000
#define DNS_PORT 53

enum APNetworkStatus
{
    ACTIVE = 0,
    INACTIVE
};

class APSettings
{
public:
    static constexpr size_t kMinSsidLength = 3;
    static constexpr size_t kMaxSsidLength = 32;
    static constexpr size_t kMinPasswordLength = 8;
    static constexpr size_t kMaxPasswordLength = 63;
    static constexpr uint8_t kMinChannel = 1;
    static constexpr uint8_t kMaxChannel = 13;
    static constexpr uint8_t kMinClients = 1;
    static constexpr uint8_t kMaxClients = 8;

    String ssid;
    String password;
    uint8_t channel;
    bool ssidHidden;
    uint8_t maxClients;

    IPAddress localIP;
    IPAddress gatewayIP;
    IPAddress subnetMask;

    bool operator==(const APSettings &settings) const
    {
        return ssid == settings.ssid && password == settings.password &&
               channel == settings.channel && ssidHidden == settings.ssidHidden && maxClients == settings.maxClients &&
               localIP == settings.localIP && gatewayIP == settings.gatewayIP && subnetMask == settings.subnetMask;
    }

    static void read(APSettings &settings, JsonObject &root)
    {
        root["ssid"] = settings.ssid;
        root["password"] = settings.password;
        root["channel"] = settings.channel;
        root["ssid_hidden"] = settings.ssidHidden;
        root["max_clients"] = settings.maxClients;
        root["local_ip"] = settings.localIP.toString();
        root["gateway_ip"] = settings.gatewayIP.toString();
        root["subnet_mask"] = settings.subnetMask.toString();
    }

    static StateHandlerResult validate(PsychicRequest *request, JsonObject &root)
    {
        (void)request;
        APSettings candidate = fromJson(root);
        const char *errorCode = nullptr;
        if (!validateSettings(candidate, &errorCode))
        {
            return StateHandlerResult::failure(errorCode ? errorCode : "input/ap_settings_invalid", 400);
        }
        return StateHandlerResult::success();
    }

    static StateUpdateResult update(JsonObject &root, APSettings &settings, std::string_view originId)
    {
        APSettings newSettings = fromJson(root);
        const char *errorCode = nullptr;
        if (!validateSettings(newSettings, &errorCode))
        {
            // Reject bad HTTP payloads loudly, but keep boot/storage recovery paths resilient
            // by falling back to factory-safe values instead of persisting a broken AP config.
            if (originId == HTTP_ENDPOINT_ORIGIN_ID)
            {
                return StateUpdateResult::ERROR;
            }
            sanitizeForPersistence(newSettings);
        }

        if (newSettings == settings)
        {
            return StateUpdateResult::UNCHANGED;
        }
        settings = newSettings;
        return StateUpdateResult::CHANGED;
    }

private:
    static APSettings fromJson(JsonObject &root)
    {
        APSettings settings = {};
        settings.ssid = root["ssid"] | defaultSsid();
        settings.password = root["password"] | FACTORY_AP_PASSWORD;
        settings.channel = root["channel"] | FACTORY_AP_CHANNEL;
        settings.ssidHidden = root["ssid_hidden"] | FACTORY_AP_SSID_HIDDEN;
        settings.maxClients = root["max_clients"] | FACTORY_AP_MAX_CLIENTS;

        JsonUtils::readIPStr(root, "local_ip", settings.localIP, FACTORY_AP_LOCAL_IP);
        JsonUtils::readIPStr(root, "gateway_ip", settings.gatewayIP, FACTORY_AP_GATEWAY_IP);
        JsonUtils::readIPStr(root, "subnet_mask", settings.subnetMask, FACTORY_AP_SUBNET_MASK);
        return settings;
    }

    static bool validateSettings(const APSettings &settings, const char **errorCode = nullptr)
    {
        if (settings.ssid.length() < kMinSsidLength || settings.ssid.length() > kMaxSsidLength)
        {
            setErrorCode(errorCode, "input/ap_ssid_invalid");
            return false;
        }

        if (!settings.password.isEmpty() &&
            (settings.password.length() < kMinPasswordLength || settings.password.length() > kMaxPasswordLength))
        {
            setErrorCode(errorCode, "input/ap_password_invalid");
            return false;
        }

        if (settings.channel < kMinChannel || settings.channel > kMaxChannel)
        {
            setErrorCode(errorCode, "input/ap_channel_invalid");
            return false;
        }

        if (settings.maxClients < kMinClients || settings.maxClients > kMaxClients)
        {
            setErrorCode(errorCode, "input/ap_max_clients_invalid");
            return false;
        }

        if (!isValidConfiguredIp(settings.localIP))
        {
            setErrorCode(errorCode, "input/ap_local_ip_invalid");
            return false;
        }

        if (!isValidConfiguredIp(settings.gatewayIP))
        {
            setErrorCode(errorCode, "input/ap_gateway_ip_invalid");
            return false;
        }

        if (!isValidConfiguredIp(settings.subnetMask))
        {
            setErrorCode(errorCode, "input/ap_subnet_mask_invalid");
            return false;
        }

        return true;
    }

    static void sanitizeForPersistence(APSettings &settings)
    {
        // Stored config may come from an older build or a corrupted file, so repair fields
        // individually instead of dropping the whole object when AP mode is needed.
        if (settings.ssid.length() < kMinSsidLength || settings.ssid.length() > kMaxSsidLength)
        {
            settings.ssid = defaultSsid();
        }

        if (!settings.password.isEmpty() &&
            (settings.password.length() < kMinPasswordLength || settings.password.length() > kMaxPasswordLength))
        {
            settings.password = FACTORY_AP_PASSWORD;
        }

        if (settings.channel < kMinChannel || settings.channel > kMaxChannel)
        {
            settings.channel = FACTORY_AP_CHANNEL;
        }

        if (settings.maxClients < kMinClients || settings.maxClients > kMaxClients)
        {
            settings.maxClients = FACTORY_AP_MAX_CLIENTS;
        }

        if (!isValidConfiguredIp(settings.localIP))
        {
            settings.localIP = factoryIp(FACTORY_AP_LOCAL_IP);
        }

        if (!isValidConfiguredIp(settings.gatewayIP))
        {
            settings.gatewayIP = factoryIp(FACTORY_AP_GATEWAY_IP);
        }

        if (!isValidConfiguredIp(settings.subnetMask))
        {
            settings.subnetMask = factoryIp(FACTORY_AP_SUBNET_MASK);
        }
    }

    static bool isValidConfiguredIp(const IPAddress &ip)
    {
        return ip != IPAddress(INADDR_NONE) && ip != IPAddress(static_cast<uint32_t>(0));
    }

    static IPAddress factoryIp(const char *value)
    {
        IPAddress ip;
        ip.fromString(value);
        return ip;
    }

    static String defaultSsid()
    {
        return SettingValue::format(FACTORY_AP_SSID);
    }

    static void setErrorCode(const char **errorCode, const char *value)
    {
        if (errorCode)
        {
            *errorCode = value;
        }
    }
};

class APSettingsService : public StatefulService<APSettings>
{
public:
    APSettingsService(PsychicHttpServer *server, FS *fs, SecurityManager *securityManager);

    void begin();
    void loop();
    APNetworkStatus getAPNetworkStatus();
    bool isAccessPointStarted() const { return _apStarted; }

    void forceAPMode();
    bool startAccessPoint();
    void stopAccessPoint();

private:
    PsychicHttpServer *_server;
    SecurityManager *_securityManager;
    HttpEndpoint<APSettings> _httpEndpoint;
    FSPersistence<APSettings> _fsPersistence;

    // for the captive portal
    DNSServer *_dnsServer;
    bool _dnsServerAllocatedInPsram = false;

    bool _apStarted = false;

    void ensureLoadedState();
    void stopDnsServer();
    void stopAP();
    void handleDNS();
};

#endif // end APSettingsConfig_h
