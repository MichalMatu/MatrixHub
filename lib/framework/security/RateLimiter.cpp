#include "RateLimiter.h"
#include "../../src/system/utils/ScopeLock.h"

RateLimiter::RateLimiter(size_t maxRequests, unsigned long windowSizeMs)
    : _maxRequests(maxRequests), _windowSizeMs(windowSizeMs), _lastCleanup(0) {
    _mutex = xSemaphoreCreateMutex();
}

RateLimiter::~RateLimiter() {
    if (_mutex) {
        vSemaphoreDelete(_mutex);
    }
}

#ifndef NATIVE_BUILD
bool RateLimiter::shouldAllow(const IPAddress& ip) {
    return shouldAllow((uint32_t)ip);
}
#endif

bool RateLimiter::shouldAllow(uint32_t ip) {
    if (!_mutex) return true;
    
    SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(100)); // Optimized timeout
    if (!lock.isLocked()) return false; 
    
    uint32_t now = millis();

    // Periodic cleanup (every 1 minute)
    if (now - _lastCleanup > 60000) {
        cleanup(false);
        _lastCleanup = now;
    }

    auto it = _clients.find(ip);
    if (it == _clients.end()) {
        if (_clients.size() >= 1000) { 
            cleanup(true); // Aggressive cleanup when full
            if (_clients.size() >= 1000) {
                // Eviction Policy: Remove the oldest entry to make room
                auto oldest = _clients.begin();
                for (auto it_e = _clients.begin(); it_e != _clients.end(); ++it_e) {
                    if (it_e->second.lastRefill < oldest->second.lastRefill) {
                        oldest = it_e;
                    }
                }
                if (oldest != _clients.end()) {
                    _clients.erase(oldest);
                }
            }
        }
        // New client: grant initial tokens (max - 1) and record time
        _clients[ip] = {(float)_maxRequests - 1, now};
        return true;
    }

    TokenBucket& bucket = it->second;

    // Refill tokens: (time_elapsed / windowSize) * maxRequests
    float refill = (float)(now - bucket.lastRefill) * (float)_maxRequests / (float)_windowSizeMs;
    bucket.tokens = min((float)_maxRequests, bucket.tokens + refill);
    bucket.lastRefill = now;

    if (bucket.tokens >= 1.0f) {
        bucket.tokens -= 1.0f;
        return true;
    } else {
        return false;
    }
}

void RateLimiter::cleanup(bool force) {
    uint32_t now = millis();
    // Threshold depends on force parameter: 1x window (aggressive) or 2x window (standard)
    unsigned long threshold = force ? _windowSizeMs : (_windowSizeMs * 2);
    
    for (auto it = _clients.begin(); it != _clients.end(); ) {
        if (now - it->second.lastRefill > threshold) {
            it = _clients.erase(it);
        } else {
            ++it;
        }
    }
}
