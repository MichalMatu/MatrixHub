#ifndef PsramAllocator_h
#define PsramAllocator_h

#include <Arduino.h>
#include <cstddef>
#include <limits>
#include <new>
#include <esp_heap_caps.h>

/**
 * STL-compatible allocator that prefers PSRAM (SPIRAM) and falls back to the
 * default heap if PSRAM is unavailable or exhausted.
 * Use this with std::vector, std::map, etc. when you want to save internal DRAM.
 */
template <class T>
struct PsramAllocator {
    using value_type = T;

    PsramAllocator() = default;
    
    template <class U>
    constexpr PsramAllocator(const PsramAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_alloc();

        // Match Arduino's ps_malloc() capability flags for generic byte-addressable PSRAM data.
        void* p = heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        
        if (!p) {
            // Fallback to internal RAM if PSRAM is full or not available?
            // For now, let's try standard malloc as fallback or throw
            p = malloc(n * sizeof(T));
        }
        
        if (!p) throw std::bad_alloc();
        
        return static_cast<T*>(p);
    }

    void deallocate(T* p, std::size_t) noexcept {
        // heap_caps_free handles pointers from any heap region correctly
        heap_caps_free(p);
        // or just free(p) - implementation dependent but free() usually works too
    }
};

template <class T, class U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) { return true; }

template <class T, class U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) { return false; }

#endif
