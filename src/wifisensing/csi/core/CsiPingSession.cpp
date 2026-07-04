#include <Arduino.h>
#include "CsiPingSession.h"
#include "../../../system/logging/Logging.h" 
#include <errno.h>
#include <fcntl.h>

#undef LOG_TAG
#define LOG_TAG "CsiPing"

namespace WIFISENSING {
namespace CSI {

#ifndef ICMP_ECHO
#define ICMP_ECHO 8
#endif

void CsiPingSession::start() {
    if (_socket >= 0) return;
    
    // The raw socket is only a traffic generator. We do not read ICMP replies
    // here; CSI samples arrive independently through the Wi-Fi driver's RX callback.
    _socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (_socket < 0) {
        LOGE("Failed to create Raw ICMP Socket: %d", errno);
        return;
    }

    // [FIX] Add hard timeouts to prevent task starvation during high LwIP load (e.g. TLS)
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000; // 50ms hard limit
    if (setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        LOGE("Failed to set SO_RCVTIMEO");
    }
    if (setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        LOGE("Failed to set SO_SNDTIMEO");
    }

    memset(&_target, 0, sizeof(_target));
    _target.sin_family = AF_INET;
    _target.sin_len = sizeof(_target);
    refreshGatewayTarget(true);

    int flags = fcntl(_socket, F_GETFL, 0);
    fcntl(_socket, F_SETFL, flags | O_NONBLOCK);

    LOGI("Raw Ping Socket Started (FD: %d, Target: %s)", _socket, WiFi.gatewayIP().toString().c_str());
}

void CsiPingSession::stop() {
    if (_socket >= 0) {
        close(_socket);
        _socket = -1;
        LOGI("Raw Ping Socket Closed");
    }
    memset(&_target, 0, sizeof(_target));
}

uint16_t CsiPingSession::calculateChecksum(void *b, int len) {
    uint16_t *buf = (uint16_t *)b;
    uint32_t sum = 0;
    for (sum = 0; len > 1; len -= 2) sum += *buf++;
    if (len == 1) sum += *(uint8_t *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ~sum;
}

bool CsiPingSession::refreshGatewayTarget(bool logChanges) {
    const uint32_t previousGateway = _target.sin_addr.s_addr;
    const uint32_t currentGateway = (uint32_t)WiFi.gatewayIP();

    if (currentGateway == 0) {
        _target.sin_addr.s_addr = 0;
        return false;
    }

    if (previousGateway != currentGateway) {
        _target.sin_addr.s_addr = currentGateway;
        if (logChanges) {
            LOGI("CSI ping target updated to gateway: %s", WiFi.gatewayIP().toString().c_str());
        }
    }

    return true;
}

void CsiPingSession::send() {
    if (_socket < 0) return;
    // Basic safety check
    if (!WiFi.isConnected()) return;

    const size_t payload_len = 32;
    struct {
        struct icmp_echo_hdr hdr;
        uint8_t payload[payload_len];
    } packet;

    memset(&packet, 0, sizeof(packet));
    packet.hdr.type = ICMP_ECHO;
    packet.hdr.code = 0;
    packet.hdr.id = 0xAFAF;
    packet.hdr.seqno = htons(_seqNum++);
    
    for (size_t i = 0; i < payload_len; i++) packet.payload[i] = (uint8_t)i;
    packet.hdr.chksum = 0;
    packet.hdr.chksum = calculateChecksum(&packet, sizeof(packet));

    // Echo replies from the current gateway provide a simple, repeatable source
    // of inbound Wi-Fi frames that can trigger CSI capture even on an otherwise idle LAN.
    // Re-evaluate the current gateway on every send so reconnect/roam does not
    // leave the raw socket pinned to a stale LAN target.
    if (!refreshGatewayTarget(false)) {
        return;
    }

    int sent = sendto(_socket, &packet, sizeof(packet), 0, (struct sockaddr *)&_target, sizeof(_target));
    if (sent < 0 && errno != EAGAIN) {
        LOGW("Ping Send Fail: %d", errno);
    }
}

} // namespace CSI
} // namespace WIFISENSING
