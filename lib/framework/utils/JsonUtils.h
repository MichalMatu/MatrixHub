#ifndef JsonUtils_h
#define JsonUtils_h

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
#include <utils/IPUtils.h>
#include <ArduinoJson.h>

class JsonUtils
{
public:
    static void readIPStr(JsonObject &root, const String &key, IPAddress &ip, const String &def)
    {
        IPAddress defaultIp = {};
        if (!defaultIp.fromString(def))
        {
            defaultIp = INADDR_NONE;
        }
        readIP(root, key, ip, defaultIp);
    }

    static void readIP(JsonObject &root, const String &key, IPAddress &ip, const IPAddress &defaultIp = IPAddress(INADDR_NONE))
    {
        const char *ipStr = root[key].as<const char *>();
        if (!ipStr || !ip.fromString(ipStr))
        {
            ip = defaultIp;
        }
    }

    static void writeIP(JsonObject &root, const String &key, const IPAddress &ip)
    {
        if (IPUtils::isSet(ip))
        {
            root[key] = ip.toString();
        }
    }
};

#endif // end JsonUtils
