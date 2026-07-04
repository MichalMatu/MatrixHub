#ifndef RateLimiter_h
#define RateLimiter_h

#include <Arduino.h>
#ifndef NATIVE_BUILD
#include <utils/PsramAllocator.h>
#endif
#include <map>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct TokenBucket {
    float tokens;
    uint32_t lastRefill;
};

/**
 * Thread-safe Rate Limiter with fixed-window counter algorithm.
 * Protects against brute-force and DoS attacks by limiting requests per IP.
 */
class RateLimiter {
public:
    RateLimiter(size_t maxRequests, unsigned long windowSizeMs);
    ~RateLimiter();

    /**
     * Check if a request from the given IP should be allowed.
     * @param ip Client IP address as uint32_t (IPv4)
     * @return true if allowed, false if rate limited
     */
    bool shouldAllow(uint32_t ip);
    
    /**
     * Convenience overload for String IP (converts internally)
     */
#ifndef NATIVE_BUILD
    bool shouldAllow(const IPAddress& ip);
#endif

    /**
     * Remove expired entries to free memory.
     * @param force If true, use a more aggressive threshold (1x window instead of 2x)
     */
    void cleanup(bool force = false);

private:
    size_t _maxRequests;
    unsigned long _windowSizeMs;
#ifdef NATIVE_BUILD
    std::map<uint32_t, TokenBucket> _clients;
#else
    std::map<uint32_t, TokenBucket, std::less<uint32_t>, PsramAllocator<std::pair<const uint32_t, TokenBucket>>> _clients;
#endif
    unsigned long _lastCleanup;
    SemaphoreHandle_t _mutex;
};

#endif
