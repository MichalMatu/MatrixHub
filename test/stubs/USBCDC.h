#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>

inline std::string g_usb_cdc_written;
inline std::deque<char> g_usb_cdc_read_buffer;

class USBCDC {
public:
    void begin() {}
    void end() {}
    void flush() {}

    size_t write(const uint8_t* data, size_t len) {
        if (data && len) {
            g_usb_cdc_written.append(reinterpret_cast<const char*>(data), len);
        }
        return len;
    }

    size_t write(uint8_t value) {
        g_usb_cdc_written.push_back(static_cast<char>(value));
        return 1;
    }

    int available() const {
        return static_cast<int>(g_usb_cdc_read_buffer.size());
    }

    int read() {
        if (g_usb_cdc_read_buffer.empty()) {
            return -1;
        }

        const unsigned char ch = static_cast<unsigned char>(g_usb_cdc_read_buffer.front());
        g_usb_cdc_read_buffer.pop_front();
        return ch;
    }

    size_t availableForWrite() const {
        return 4096;
    }

    explicit operator bool() const {
        return true;
    }
};
