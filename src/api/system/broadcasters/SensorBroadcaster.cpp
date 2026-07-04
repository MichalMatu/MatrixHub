#include "SensorBroadcaster.h"
#include "SensorTelemetryPacket.h"
#include "../../../sensors/SensorLoggingTask.h"
#include "../../../wifisensing/WifiSensingService.h"
#include "../../../shelly/ShellyTypes.h"
#include "../../../system/logging/Logging.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "SensorBroadcaster"

namespace API {

SensorBroadcaster::SensorBroadcaster() {}

SensorBroadcaster::~SensorBroadcaster() {}

void SensorBroadcaster::begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, PsychicHttpServer* server, WIFISENSING::WifiSensingService* wifiSensing) {
    _systemWs = systemWs;
    _channels = channels;
    _server = server;

    // 1. Register Sensor Updates (Observer)
    bool callbackRegistered = false;
    for (uint8_t attempt = 0; attempt < 5 && !callbackRegistered; ++attempt) {
        callbackRegistered = SensorLoggingTask::setUpdateCallback([this](const SensorSnapshot& snap, bool lastReadOk) {
        if (!_systemWs || !_systemWs->hasClients()) return;

        // Only send if someone subscribed to TELEMETRY channel
        if (_channels && !_channels->hasSubscribers(ChannelSubscriptions::TELEMETRY)) return;
        
        // Rate limit to 1000ms
        uint32_t now = millis();
        if (now - _lastSensorBroadcastMs < 1000) return;
        _lastSensorBroadcastMs = now;
        
        // Broadcast "sensor" event (Binary)
        // [0]    Magic 0x54 ('T')
        // [1-2]  CO2 (uint16)
        // [3-4]  Temp * 10 (int16)
        // [5-6]  Humid * 10 (uint16)
        // [7-10] Timestamp (uint32)
        // [11]   Flags (bit0=lastReadOk)
        //
        // The extra status byte keeps old payload semantics for the numeric
        // values but lets the dashboard notice hard read failures immediately
        // instead of waiting for a reconnect-time JSON snapshot.
        uint8_t buf[kSensorTelemetryPacketSize];
        const size_t off = encodeSensorTelemetryPacket(snap, lastReadOk, buf, sizeof(buf));
        if (off == 0) return;

        if (_channels && _server && _server->server) {
            _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::TELEMETRY, buf, off);
        }
        });
        if (!callbackRegistered) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    if (!callbackRegistered) {
        LOGE("Failed to register sensor update callback after retries");
    }

    // 2. Register WiFi Sensing Updates (Observer)
    // 2. Register WiFi Sensing Updates (Observer)
    if (wifiSensing) {
        wifiSensing->addSensingCallback(
        [this](const WIFISENSING::RssiSample& sample, const WIFISENSING::RssiStats& stats, bool isMotion) {
            // Rate limit to 500ms
            uint32_t now = millis();
            if (now - _lastSensingBroadcastMs < 500) return;
            _lastSensingBroadcastMs = now;

            // Only send if someone subscribed to SENSING channel
            if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::SENSING)) return;

            // Binary packet (11 bytes):
            uint8_t packet[11];
            packet[0] = 0x57; // 'W'
            memcpy(&packet[1], &sample.timestampMs, 4);
            packet[5] = (uint8_t)sample.rssi;
            memcpy(&packet[6], &stats.variance, 4);
            packet[10] = isMotion ? 1 : 0;

            if (_server && _server->server) {
                // Broadcast sensing data (rate limited)
                _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::SENSING, packet, 11);
            }
        });
    }
}

void SensorBroadcaster::sendShellyEvent(const void* devicePtr) {
    // Only send to clients subscribed to Shelly channel
    if (!_channels || !_channels->hasSubscribers(ChannelSubscriptions::SHELLY)) return;

    const auto* dev = static_cast<const SHELLY::ShellyDevice*>(devicePtr);
    
    // Binary Shelly Protocol
    uint8_t packet[32];
    size_t offset = 0;
    
    // Magic byte
    packet[offset++] = 0x53;  // 'S' for Shelly
    
    // ID (variable length, max 8)
    size_t idLen = strlen(dev->id);
    if (idLen > 8) idLen = 8;
    packet[offset++] = (uint8_t)idLen;
    memcpy(&packet[offset], dev->id, idLen);
    offset += idLen;
    
    // Flags
    uint8_t flags = 0;
    if (dev->isOnline) flags |= 0x01;
    if (dev->isOn) flags |= 0x02;
    packet[offset++] = flags;
    
    // Power * 100 (fixed-point)
    uint16_t power = (uint16_t)(dev->power * 100.0f);
    memcpy(&packet[offset], &power, 2);
    offset += 2;
    
    // Voltage * 10
    uint16_t voltage = (uint16_t)(dev->voltage * 10.0f);
    memcpy(&packet[offset], &voltage, 2);
    offset += 2;
    
    // Current * 1000
    uint16_t current = (uint16_t)(dev->current * 1000.0f);
    memcpy(&packet[offset], &current, 2);
    offset += 2;
    
    // Energy * 10
    uint32_t energy = (uint32_t)(dev->energy * 10.0f);
    memcpy(&packet[offset], &energy, 4);
    offset += 4;
    
    // Temperature (int8)
    packet[offset++] = (int8_t)dev->temperature;
    
    // RSSI (int8)
    packet[offset++] = (int8_t)dev->rssi;
    
    if (_server && _server->server) {
    // Broadcast generic sensor snapshot
    _channels->broadcast(_systemWs, _server->server, ChannelSubscriptions::SHELLY, packet, offset);
    }
}

} // namespace API
