#pragma once

#include "../../common/WebSocketBroadcaster.h"
#include <freertos/FreeRTOS.h>

namespace BLE { class BleService; }
namespace MACROS { class MacroService; struct MacroState; }
namespace API { class ChannelSubscriptions; }
#include <freertos/timers.h>

namespace API {

class SystemStatusBroadcaster {
public:
    SystemStatusBroadcaster();
    ~SystemStatusBroadcaster();

    void begin(WebSocketBroadcaster* systemWs, ChannelSubscriptions* channels, BLE::BleService* bleService, MACROS::MacroService* macroService);
    
    // Timer Control
    void startTimer();
    void stopTimer();

private:
    WebSocketBroadcaster* _systemWs = nullptr;
    ChannelSubscriptions* _channels = nullptr;
    BLE::BleService* _bleService = nullptr;
    TimerHandle_t _broadcastTimer = nullptr;

    uint8_t _pingCounter = 0;
    uint8_t _lastMacroStatus = 0xFF;
    uint32_t _lastMacroBroadcastMs = 0;

    void broadcastStatus();
    void broadcastMacroState(const MACROS::MacroState& state);
    static void onSystemTimer(TimerHandle_t xTimer);
};

} // namespace API
