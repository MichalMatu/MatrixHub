#pragma once
#include <vector>
#include "Arduino.h"

namespace TEST_STUBS::MDNS {
inline bool beginResult = true;
inline bool started = false;
inline int beginCalls = 0;
inline int endCalls = 0;
inline String lastHostname;
inline String lastInstanceName;
inline std::vector<String> services;
inline std::vector<String> txtItems;

inline void reset() {
    beginResult = true;
    started = false;
    beginCalls = 0;
    endCalls = 0;
    lastHostname = "";
    lastInstanceName = "";
    services.clear();
    txtItems.clear();
}
}

class MDNSResponder {
public:
    bool begin(const String& hostName) {
        TEST_STUBS::MDNS::beginCalls++;
        TEST_STUBS::MDNS::lastHostname = hostName;
        TEST_STUBS::MDNS::started = TEST_STUBS::MDNS::beginResult;
        return TEST_STUBS::MDNS::beginResult;
    }

    bool begin(const char* hostName) {
        return begin(String(hostName));
    }

    void end() {
        TEST_STUBS::MDNS::endCalls++;
        TEST_STUBS::MDNS::started = false;
    }

    void setInstanceName(const String& name) {
        TEST_STUBS::MDNS::lastInstanceName = name;
    }

    void setInstanceName(const char* name) {
        setInstanceName(String(name));
    }

    bool addService(const char* service, const char* proto, uint16_t port) {
        TEST_STUBS::MDNS::services.push_back(
            String(service ? service : "") + ":" + String(proto ? proto : "") + ":" + String(static_cast<int>(port)));
        return true;
    }

    void addServiceTxt(const char* service, const char* proto, const char* key, const char* value) {
        TEST_STUBS::MDNS::txtItems.push_back(
            String(service ? service : "") + ":" + String(proto ? proto : "") + ":" +
            String(key ? key : "") + ":" + String(value ? value : ""));
    }
};

inline MDNSResponder MDNS;
