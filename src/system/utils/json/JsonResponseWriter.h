#pragma once
#include <esp_http_server.h>
#include <Arduino.h>

namespace Utils {

class JsonResponseWriter {
public:
    // Call once at startup to allocate PSRAM buffer
    static bool begin();
    
    explicit JsonResponseWriter(httpd_req_t* req);

    bool beginResponse(); // Sets application/json and starts chunked response
    
    bool raw(const char* str);
    bool raw(const char* data, size_t len);
    bool fmt(const char* format, ...);
    
    bool string(const char* s); // Writes "escaped_string"
    bool key(const char* k);    // Writes "key":
    
    // Primitives
    bool value(bool v);
    bool value(int v);
    bool value(unsigned int v);
    bool value(long v);
    bool value(unsigned long v);
    bool value(long long v);
    bool value(unsigned long long v);
    bool value(float v, int decimals = 2);
    bool value(double v, int decimals = 2);
    
    bool finish(); // Sends final empty chunk

private:
    httpd_req_t* _req;
    size_t _stagingLen = 0;
    char _stagingBuf[1024]{0};

    bool flushPending();
    bool sendEscaped(const char* s);
};

} // namespace Utils
