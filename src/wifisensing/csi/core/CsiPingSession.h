#pragma once

#include <lwip/sockets.h>
#include <lwip/inet.h>
#include <lwip/icmp.h>
#include <Arduino.h>
#include <WiFi.h>

namespace WIFISENSING {
namespace CSI {

class CsiPingSession {
public:
    CsiPingSession() = default;
    ~CsiPingSession() { stop(); }

    void start();
    void stop();
    void send();

    bool isActive() const { return _socket >= 0; }

private:
    int _socket = -1;
    struct sockaddr_in _target;
    uint16_t _seqNum = 0;

    static uint16_t calculateChecksum(void *b, int len);
    bool refreshGatewayTarget(bool logChanges);
};

} // namespace CSI
} // namespace WIFISENSING
