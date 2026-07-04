#pragma once
#include <Arduino.h>
#include <string>

class NetworkClient : public Stream {
public:
    std::string _inputBuffer;
    size_t _readPos = 0;
    bool _connected = false;
    int32_t _timeout = 0;
    uint32_t _lastReadTimeout = 0;
    uint32_t _lastWriteTimeout = 0;

    void setMockData(const std::string& data) {
        _inputBuffer = data;
        _readPos = 0;
        _connected = true;
    }

    int available() override { return static_cast<int>(_inputBuffer.size() - _readPos); }
    
    int read() override {
        if (_readPos < _inputBuffer.size()) {
            return static_cast<unsigned char>(_inputBuffer[_readPos++]);
        }
        return -1;
    }
    
    int peek() override {
        if (_readPos < _inputBuffer.size()) {
            return static_cast<unsigned char>(_inputBuffer[_readPos]);
        }
        return -1;
    }

    void flush() override {}

    size_t write(uint8_t c) override {
        _inputBuffer.push_back(static_cast<char>(c));
        return 1;
    }

    size_t write(const uint8_t* buffer, size_t size) {
        if (!buffer || size == 0) {
            return 0;
        }
        _inputBuffer.append(reinterpret_cast<const char*>(buffer), size);
        return size;
    }

    uint8_t connected() { return _connected ? 1 : 0; }

    void stop() {
        _connected = false;
    }

    using Print::write;
};
