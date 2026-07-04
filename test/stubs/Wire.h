#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

class MockWire {
public:
    void reset() {
        endTransmissionResult = 0;
        queuedResultCount = 0;
        queuedResultIndex = 0;
        beginCalls = 0;
        endCalls = 0;
    }

    void queueEndTransmissionResult(int result) {
        if (queuedResultCount < kMaxQueuedResults) {
            queuedResults[queuedResultCount++] = result;
        }
    }

    void setEndTransmissionResult(int result) {
        endTransmissionResult = result;
    }

    void begin() {}
    void begin(int sda, int scl) {
        (void)sda;
        (void)scl;
        ++beginCalls;
    }
    void end() { ++endCalls; }
    void setClock(uint32_t hz) { (void)hz; }
    void setTimeOut(uint16_t timeoutMs) { (void)timeoutMs; }
    void beginTransmission(int address) { (void)address; }
    size_t write(uint8_t value) {
        (void)value;
        return 1;
    }
    size_t write(const uint8_t* buffer, size_t size) {
        (void)buffer;
        return size;
    }
    int endTransmission(bool stopBit = true) {
        (void)stopBit;
        if (queuedResultIndex < queuedResultCount) {
            return queuedResults[queuedResultIndex++];
        }
        return endTransmissionResult;
    }
    int requestFrom(int address, int quantity) {
        (void)address;
        return quantity;
    }
    int requestFrom(int address, int quantity, int stopBit) {
        (void)address;
        (void)stopBit;
        return quantity;
    }
    size_t readBytes(uint8_t* buffer, size_t length) {
        if (buffer) {
            std::memset(buffer, 0, length);
        }
        return length;
    }
    int available() const { return 0; }
    int read() { return -1; }

    int beginCalls = 0;
    int endCalls = 0;

private:
    static constexpr size_t kMaxQueuedResults = 16;
    int endTransmissionResult = 0;
    int queuedResults[kMaxQueuedResults]{};
    size_t queuedResultCount = 0;
    size_t queuedResultIndex = 0;
};

inline MockWire Wire;
inline MockWire Wire1;
using TwoWire = MockWire;
