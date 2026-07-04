#pragma once

#include <Arduino.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>

namespace TEST_STUBS::WIFIUDP {

inline bool beginPacketResult = true;
inline bool endPacketResult = true;
inline std::string lastHost;
inline uint16_t lastPort = 0;
inline std::string lastPayload;
inline size_t beginCalls = 0;
inline size_t writeCalls = 0;
inline size_t endCalls = 0;
inline uint32_t sendDelayMs = 0;
inline std::atomic<int> activeSends{0};
inline std::atomic<int> maxConcurrentSends{0};

inline void reset() {
    beginPacketResult = true;
    endPacketResult = true;
    lastHost.clear();
    lastPort = 0;
    lastPayload.clear();
    beginCalls = 0;
    writeCalls = 0;
    endCalls = 0;
    sendDelayMs = 0;
    activeSends.store(0);
    maxConcurrentSends.store(0);
}

inline void updateMaxConcurrent(int active) {
    int current = maxConcurrentSends.load();
    while (active > current && !maxConcurrentSends.compare_exchange_weak(current, active)) {
    }
}

}  // namespace TEST_STUBS::WIFIUDP

class WiFiUDP {
public:
    bool beginPacket(const char* host, uint16_t port) {
        TEST_STUBS::WIFIUDP::beginCalls++;
        TEST_STUBS::WIFIUDP::lastHost = host ? host : "";
        TEST_STUBS::WIFIUDP::lastPort = port;
        return TEST_STUBS::WIFIUDP::beginPacketResult;
    }

    size_t write(const uint8_t* buffer, size_t size) {
        TEST_STUBS::WIFIUDP::writeCalls++;
        TEST_STUBS::WIFIUDP::lastPayload.assign(
            reinterpret_cast<const char*>(buffer),
            reinterpret_cast<const char*>(buffer) + size);
        return size;
    }

    bool endPacket() {
        TEST_STUBS::WIFIUDP::endCalls++;
        const int active = TEST_STUBS::WIFIUDP::activeSends.fetch_add(1) + 1;
        TEST_STUBS::WIFIUDP::updateMaxConcurrent(active);
        if (TEST_STUBS::WIFIUDP::sendDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(TEST_STUBS::WIFIUDP::sendDelayMs));
        }
        TEST_STUBS::WIFIUDP::activeSends.fetch_sub(1);
        return TEST_STUBS::WIFIUDP::endPacketResult;
    }
};
