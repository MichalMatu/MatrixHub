#include "UdpPacketFormatter.h"
#include <stdio.h>

namespace UDPPUSH {

size_t UdpPacketFormatter::formatLineProtocol(char* buffer, size_t bufferSize, const SensorSnapshot& snap) {
    const bool valid = (snap.timestamp_ms != 0);
    
    if (valid) {
        // Round to 2 decimal places properly
        float roundedTemp = round(snap.temp * 100.0f) / 100.0f;
        float roundedHumid = round(snap.humid * 100.0f) / 100.0f;
        
        int tempInt = (int)roundedTemp;
        int tempFrac = abs((int)round(roundedTemp * 100.0f) % 100);
        int humInt = (int)roundedHumid;
        int humFrac = abs((int)round(roundedHumid * 100.0f) % 100);
        
        // Handle negative zero edge case (-0.xx)
        const char* tempSign = (roundedTemp < 0 && tempInt == 0) ? "-" : "";

        int written = snprintf(buffer, bufferSize,
            "sensors,device=matrixhub,status=ok co2=%ui,temp=%s%d.%02d,humidity=%d.%02d,seq=%u",
            snap.co2, tempSign, tempInt, tempFrac, humInt, humFrac, snap.seq);
        return (written > 0 && (size_t)written < bufferSize) ? written : 0;
    } else {
        int written = snprintf(buffer, bufferSize,
            "sensors,device=matrixhub,status=missing seq=%u", 
            snap.seq);
        return (written > 0 && (size_t)written < bufferSize) ? written : 0;
    }
}

size_t UdpPacketFormatter::formatJson(char* buffer, size_t bufferSize, const SensorSnapshot& snap) {
    const bool valid = (snap.timestamp_ms != 0);

    if (valid) {
        float roundedTemp = round(snap.temp * 100.0f) / 100.0f;
        float roundedHumid = round(snap.humid * 100.0f) / 100.0f;
        
        int tempInt = (int)roundedTemp;
        int tempFrac = abs((int)round(roundedTemp * 100.0f) % 100);
        int humInt = (int)roundedHumid;
        int humFrac = abs((int)round(roundedHumid * 100.0f) % 100);

        const char* tempSign = (roundedTemp < 0 && tempInt == 0) ? "-" : "";

        int written = snprintf(buffer, bufferSize,
            "{\"status\":\"ok\",\"co2\":%u,\"temp\":%s%d.%02d,\"humidity\":%d.%02d,\"seq\":%u}",
            snap.co2, tempSign, tempInt, tempFrac, humInt, humFrac, snap.seq);
        return (written > 0 && (size_t)written < bufferSize) ? written : 0;
    } else {
        int written = snprintf(buffer, bufferSize,
            "{\"status\":\"missing\",\"seq\":%u}",
            snap.seq);
        return (written > 0 && (size_t)written < bufferSize) ? written : 0;
    }
}

size_t UdpPacketFormatter::formatCsv(char* buffer, size_t bufferSize, const SensorSnapshot& snap) {
    const bool valid = (snap.timestamp_ms != 0);
    
    if (valid) {
        float roundedTemp = round(snap.temp * 100.0f) / 100.0f;
        float roundedHumid = round(snap.humid * 100.0f) / 100.0f;
        
        int tempInt = (int)roundedTemp;
        int tempFrac = abs((int)round(roundedTemp * 100.0f) % 100);
        int humInt = (int)roundedHumid;
        int humFrac = abs((int)round(roundedHumid * 100.0f) % 100);

        const char* tempSign = (roundedTemp < 0 && tempInt == 0) ? "-" : "";

        int written = snprintf(buffer, bufferSize,
            "ok,%u,%s%d.%02d,%d.%02d,%u",
            snap.co2, tempSign, tempInt, tempFrac, humInt, humFrac, snap.seq);
        return (written > 0 && (size_t)written < bufferSize) ? written : 0;
    } else {
        int written = snprintf(buffer, bufferSize, "missing,0,0.00,0.00,%u", snap.seq);
        return (written > 0 && (size_t)written < bufferSize) ? written : 0;
    }
}

} // namespace UDPPUSH
