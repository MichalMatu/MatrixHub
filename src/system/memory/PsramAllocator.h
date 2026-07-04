/**
 * @file PsramAllocator.h
 * @brief Standard Allocator for STL containers to use PSRAM
 */

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <ArduinoJson.h>
#include <ArduinoJson/Variant/VariantData.hpp>
#include <vector>
#include <string>

namespace SYSTEM {

template <class T>
struct PsramAllocator {
    typedef T value_type;

    PsramAllocator() = default;
    
    template <class U>
    constexpr PsramAllocator(const PsramAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > (std::numeric_limits<std::size_t>::max)() / sizeof(T)) {
            ESP_LOGE("PsramAlloc", "Alloc size overflow or too large: %u * %u", (unsigned)n, (unsigned)sizeof(T));
            throw std::bad_alloc();
        }
        
        void* ptr = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ptr) {
            ESP_LOGE("PsramAlloc", "MALLOC FAILED: %u bytes", (unsigned)(n * sizeof(T)));
            throw std::bad_alloc();
        }
        return static_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept {
        free(p);
    }
};

template <class T, class U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) { return true; }

template <class T, class U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) { return false; }

using PsramString = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;
template <typename T> using PsramVector = std::vector<T, PsramAllocator<T>>;

inline PsramString makePsramString(const char* text) {
    if (!text || !*text) return PsramString(PsramAllocator<char>());
    return PsramString(text, PsramAllocator<char>());
}
inline PsramString makePsramString(const std::string& text) {
    return PsramString(text.c_str(), PsramAllocator<char>());
}
inline PsramString makePsramString(const String& text) {
    return PsramString(text.c_str(), PsramAllocator<char>());
}

// --- ARDUINOJSON V7 PSRAM ALLOCATOR ---

class JsonPsramAllocator : public ArduinoJson::Allocator {
public:
    void* allocate(size_t size) override {
        // PSRAM-only allocation
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!ptr) {
             ESP_LOGE("PsramAlloc", "PSRAM allocation failed: %u bytes", (unsigned)size);
        }
        return ptr;
    }

    void deallocate(void* ptr) override {
        free(ptr);
    }

    void* reallocate(void* ptr, size_t new_size) override {
        if (new_size == 0) {
            free(ptr);
            return nullptr;
        }

        void* new_ptr = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!new_ptr) {
            ESP_LOGE("PsramAlloc", "PSRAM reallocation failed: %u bytes", (unsigned)new_size);
        }
        return new_ptr;
    }

};

class BoundedJsonPsramAllocator : public ArduinoJson::Allocator {
public:
    explicit BoundedJsonPsramAllocator(size_t limit)
        : _requestedLimit(limit), _limit(effectiveLimitFor(limit)) {}

    void* allocate(size_t size) override {
        if (size == 0) {
            return nullptr;
        }
        if (wouldExceed(size)) {
            ESP_LOGE("PsramAlloc", "PSRAM JSON limit exceeded: %u > %u bytes",
                     (unsigned)(_used + size), (unsigned)_limit);
            return nullptr;
        }

        const size_t totalSize = allocationSize(size);
        if (totalSize < size) {
            return nullptr;
        }

        void* raw = heap_caps_malloc(totalSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!raw) {
            ESP_LOGE("PsramAlloc", "PSRAM allocation failed: %u bytes", (unsigned)size);
            return nullptr;
        }

        setStoredSize(raw, size);
        _used += size;
        _peak = std::max(_peak, _used);
        return userFromRaw(raw);
    }

    void deallocate(void* ptr) override {
        if (!ptr) {
            return;
        }

        void* raw = rawFromUser(ptr);
        const size_t size = storedSize(raw);
        _used = (size <= _used) ? (_used - size) : 0;
        heap_caps_free(raw);
    }

    void* reallocate(void* ptr, size_t newSize) override {
        if (!ptr) {
            return allocate(newSize);
        }
        if (newSize == 0) {
            deallocate(ptr);
            return nullptr;
        }

        void* raw = rawFromUser(ptr);
        const size_t oldSize = storedSize(raw);
        if (newSize > oldSize && wouldExceed(newSize - oldSize)) {
            ESP_LOGE("PsramAlloc", "PSRAM JSON limit exceeded: %u > %u bytes",
                     (unsigned)(_used + (newSize - oldSize)), (unsigned)_limit);
            return nullptr;
        }

        const size_t totalSize = allocationSize(newSize);
        if (totalSize < newSize) {
            return nullptr;
        }

        void* newRaw = heap_caps_realloc(raw, totalSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!newRaw) {
            ESP_LOGE("PsramAlloc", "PSRAM reallocation failed: %u bytes", (unsigned)newSize);
            return nullptr;
        }

        setStoredSize(newRaw, newSize);
        _used = _used - oldSize + newSize;
        _peak = std::max(_peak, _used);
        return userFromRaw(newRaw);
    }

    size_t limit() const {
        return _limit;
    }

    size_t requestedLimit() const {
        return _requestedLimit;
    }

    size_t used() const {
        return _used;
    }

    size_t peak() const {
        return _peak;
    }

    static constexpr size_t minimumCapacity() {
        return ARDUINOJSON_POOL_CAPACITY * sizeof(ArduinoJson::detail::VariantData);
    }

    // ArduinoJson v7 always allocates at least one variant pool before it can
    // parse even tiny objects. Treat caller capacity as usable budget above
    // that mandatory pool so legacy limits remain bounded but still parse.
    static constexpr size_t effectiveLimitFor(size_t requestedLimit) {
        return requestedLimit == 0
                   ? 0
                   : (requestedLimit > (std::numeric_limits<size_t>::max)() - minimumCapacity()
                          ? (std::numeric_limits<size_t>::max)()
                          : requestedLimit + minimumCapacity());
    }

private:
    static constexpr size_t kAlignment = alignof(std::max_align_t);
    static constexpr size_t kHeaderSize =
        ((sizeof(size_t) + kAlignment - 1) / kAlignment) * kAlignment;

    bool wouldExceed(size_t additionalBytes) const {
        return _limit > 0 && additionalBytes > _limit - std::min(_used, _limit);
    }

    static size_t allocationSize(size_t userSize) {
        if (userSize > (std::numeric_limits<size_t>::max)() - kHeaderSize) {
            return 0;
        }
        return userSize + kHeaderSize;
    }

    static void* userFromRaw(void* raw) {
        return static_cast<uint8_t*>(raw) + kHeaderSize;
    }

    static void* rawFromUser(void* ptr) {
        return static_cast<uint8_t*>(ptr) - kHeaderSize;
    }

    static size_t storedSize(void* raw) {
        return *static_cast<size_t*>(raw);
    }

    static void setStoredSize(void* raw, size_t size) {
        *static_cast<size_t*>(raw) = size;
    }

    size_t _requestedLimit;
    size_t _limit;
    size_t _used = 0;
    size_t _peak = 0;
};

// Shared ArduinoJson allocator backed by PSRAM. This is a process-local helper
// rather than a semantic singleton service, so expose it as a free accessor.
inline JsonPsramAllocator& jsonPsramAllocator() {
    static JsonPsramAllocator allocator;
    return allocator;
}

// API compatibility wrapper (accepts size_t in constructor)
class SpiRamJsonDocument : private BoundedJsonPsramAllocator, public ArduinoJson::JsonDocument {
public:
    explicit SpiRamJsonDocument(size_t capacity = 0)
      : BoundedJsonPsramAllocator(capacity),
        ArduinoJson::JsonDocument(static_cast<ArduinoJson::Allocator*>(
            static_cast<BoundedJsonPsramAllocator*>(this))) {}

    SpiRamJsonDocument(const SpiRamJsonDocument&) = delete;
    SpiRamJsonDocument& operator=(const SpiRamJsonDocument&) = delete;
    SpiRamJsonDocument(SpiRamJsonDocument&&) = delete;
    SpiRamJsonDocument& operator=(SpiRamJsonDocument&&) = delete;

    size_t capacityLimit() const {
        return BoundedJsonPsramAllocator::limit();
    }

    size_t requestedCapacityLimit() const {
        return BoundedJsonPsramAllocator::requestedLimit();
    }

    static constexpr size_t minimumCapacity() {
        return BoundedJsonPsramAllocator::minimumCapacity();
    }

    static constexpr size_t effectiveLimitFor(size_t requestedLimit) {
        return BoundedJsonPsramAllocator::effectiveLimitFor(requestedLimit);
    }

    size_t allocatedBytes() const {
        return BoundedJsonPsramAllocator::used();
    }

    size_t peakAllocatedBytes() const {
        return BoundedJsonPsramAllocator::peak();
    }
};

} // namespace SYSTEM
