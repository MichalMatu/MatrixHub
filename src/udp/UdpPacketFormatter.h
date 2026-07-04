#pragma once

#include <stddef.h>
#include "../sensors/model/SensorTypes.h"

namespace UDPPUSH {

/**
 * @brief Pure packet formatter independent of network/UDP layer
 */
class UdpPacketFormatter {
public:
    /**
     * Format data using InfluxDB line protocol
     * sensors,device=matrixhub co2=123i,temp=24.5,humidity=50.0
     */
    static size_t formatLineProtocol(char* buffer, size_t bufferSize, const SensorSnapshot& snap);

    /**
     * Format data as JSON
     * {"co2":123,"temp":24.5,"humidity":50.0,"seq":1}
     */
    static size_t formatJson(char* buffer, size_t bufferSize, const SensorSnapshot& snap);

    /**
     * Format data as CSV
     * 123,24.5,50.0
     */
    static size_t formatCsv(char* buffer, size_t bufferSize, const SensorSnapshot& snap);
};

} // namespace UDPPUSH
