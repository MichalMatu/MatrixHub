#pragma once

#include <Arduino.h>
#include <array>
#include <arpa/inet.h>
#include <cstdint>
#include <cstdio>

#ifndef INADDR_NONE
#define INADDR_NONE ((uint32_t)0xFFFFFFFFu)
#endif

class IPAddress {
public:
    IPAddress() = default;

    IPAddress(uint32_t address) {
        assign(address);
    }

    IPAddress(uint8_t first, uint8_t second, uint8_t third, uint8_t fourth)
        : _bytes{first, second, third, fourth} {
    }

    IPAddress& operator=(uint32_t address) {
        assign(address);
        return *this;
    }

    bool operator==(const IPAddress& other) const {
        return _bytes == other._bytes;
    }

    bool operator!=(const IPAddress& other) const {
        return !(*this == other);
    }

    uint8_t operator[](size_t index) const {
        return index < _bytes.size() ? _bytes[index] : 0;
    }

    operator uint32_t() const {
        return (static_cast<uint32_t>(_bytes[0]) << 24) |
               (static_cast<uint32_t>(_bytes[1]) << 16) |
               (static_cast<uint32_t>(_bytes[2]) << 8) |
               static_cast<uint32_t>(_bytes[3]);
    }

    bool fromString(const char* address) {
        if (!address) {
            return false;
        }

        in_addr parsed{};
        if (inet_pton(AF_INET, address, &parsed) != 1) {
            return false;
        }

        const auto* bytes = reinterpret_cast<const uint8_t*>(&parsed.s_addr);
        _bytes = {bytes[0], bytes[1], bytes[2], bytes[3]};
        return true;
    }

    bool fromString(const String& address) {
        return fromString(address.c_str());
    }

    String toString() const {
        char buffer[16];
        std::snprintf(buffer,
                      sizeof(buffer),
                      "%u.%u.%u.%u",
                      static_cast<unsigned>(_bytes[0]),
                      static_cast<unsigned>(_bytes[1]),
                      static_cast<unsigned>(_bytes[2]),
                      static_cast<unsigned>(_bytes[3]));
        return String(buffer);
    }

private:
    void assign(uint32_t address) {
        _bytes = {
            static_cast<uint8_t>((address >> 24) & 0xFFu),
            static_cast<uint8_t>((address >> 16) & 0xFFu),
            static_cast<uint8_t>((address >> 8) & 0xFFu),
            static_cast<uint8_t>(address & 0xFFu)
        };
    }

    std::array<uint8_t, 4> _bytes{0, 0, 0, 0};
};
