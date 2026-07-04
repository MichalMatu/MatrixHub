#ifndef PsychicResponse_h
#define PsychicResponse_h

#include "PsychicCore.h"
#include "time.h"

class PsychicRequest;

// ESP32-C6 optimized: Fixed-size header storage instead of std::list with malloc
// This eliminates heap fragmentation from repeated small allocations per header
// Adjusted sizes to keep stack usage low (PsychicResponse is often stack-allocated)
// PRO TIP: Aligned to 128 bytes (32 + 95 + 1) for CPU cache efficiency
constexpr size_t MAX_RESPONSE_HEADERS = 6;     // Was 8
constexpr size_t MAX_HEADER_FIELD_LEN = 32;    // Standard
constexpr size_t MAX_HEADER_VALUE_LEN = 95;    // Reduced to 95 to align struct to 128 bytes

struct FixedHeader {
    char field[MAX_HEADER_FIELD_LEN];
    char value[MAX_HEADER_VALUE_LEN];
    bool used; // +1 byte = 128 bytes total
};

class PsychicResponse
{
  protected:
    PsychicRequest *_request;

    int _code;
    char _status[60];
    FixedHeader _headers[MAX_RESPONSE_HEADERS];
    size_t _headerCount;
    int64_t _contentLength;
    const char * _body;

  public:
    PsychicResponse(PsychicRequest *request);
    virtual ~PsychicResponse();

    void setCode(int code);

    void setContentType(const char *contentType);
    void setContentLength(int64_t contentLength) { _contentLength = contentLength; }
    int64_t getContentLength(int64_t contentLength) { return _contentLength; }

    void addHeader(const char *field, const char *value);

    void setCookie(const char *key, const char *value, unsigned long max_age = 60*60*24*30, const char *extras = "");

    void setContent(const char *content);
    void setContent(const uint8_t *content, size_t len);

    const char * getContent();
    size_t getContentLength();

    virtual esp_err_t send();
    void sendHeaders();
    esp_err_t sendChunk(uint8_t *chunk, size_t chunksize);
    esp_err_t finishChunking();
};

#endif // PsychicResponse_h