#pragma once

#include "../../system/datalogger/BinaryFormat.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SENSORS {

class PsramLogBuffer {
public:
    static bool begin(size_t capacityRecords);
    static void end();
    static bool push(const DATALOG::BinaryLogRecord& record);

    static bool getRecord(size_t index, DATALOG::BinaryLogRecord& outRecord);
    static size_t copyLastRecords(DATALOG::BinaryLogRecord* outBuffer, size_t maxCount);
    static bool tryCount(size_t& outCount, TickType_t timeoutTicks);
    static bool tryIsEmpty(bool& outIsEmpty, TickType_t timeoutTicks);
    static bool tryIsFull(bool& outIsFull, TickType_t timeoutTicks);
    static size_t count();
    static bool isEmpty();
    static bool isFull();
    static size_t getCapacity() { return _capacity; }

private:
    static DATALOG::BinaryLogRecord* _buffer;
    static size_t _capacity;
    static size_t _head;
    static size_t _count;
    static SemaphoreHandle_t _mutex;
};

} // namespace SENSORS
