#pragma once
#include <ArduinoJson.h>
#include "../../system/rtc/types/RtcSystemTypes.h"


namespace CONFIG {
namespace JSON {


    

    
    void deserializeHeartbeat(JsonObject& obj, RTC::HeartbeatData& data);
    void loadHeartbeat(JsonObject& obj);
    void saveHeartbeat(JsonObject& obj);
    
    void deserializeUdpPusher(JsonObject& obj, RTC::UdpPusherData& data);
    void loadUdpPusher(JsonObject& obj);
    void saveUdpPusher(JsonObject& obj);
    
    void deserializeLogging(JsonObject& obj, RTC::LoggingData& data);
    void loadLogging(JsonObject& obj);
    void saveLogging(JsonObject& obj);
    




} // namespace JSON
} // namespace CONFIG
