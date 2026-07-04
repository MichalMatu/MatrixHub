#pragma once

#include <cstdint>
#include <new>
#include <utility>

namespace SYSTEM {

/**
 * @brief Helper for managing statics allocated in specific memory sections (e.g. PSRAM)
 *        with proper alignment and lifecycle management.
 * 
 * @tparam T Service type
 */
template <typename T>
class StaticService {
public:
    // Aligned buffer for placement new
    // Using 16-byte alignment to be safe for any SIMD/Cache requirements on ESP32-S3
    // or just alignas(T) which is the standard way.
    alignas(T) uint8_t _buffer[sizeof(T)];
    T* _instance = nullptr;

    /**
     * @brief Construct the service in the static buffer
     */
    template <typename... Args>
    T* init(Args&&... args) {
        if (_instance) {
            // Already initialized - should we re-init? 
            // For safety, assume single initialization. 
            // If re-init needed, destroy first.
            return _instance;
        }
        _instance = new (_buffer) T(std::forward<Args>(args)...);
        return _instance;
    }

    /**
     * @brief Manually destroy the service (call destructor)
     */
    void destroy() {
        if (_instance) {
            _instance->~T();
            _instance = nullptr;
        }
    }

    /**
     * @brief Access the service instance
     */
    T* get() const { return _instance; }
    T* operator->() const { return _instance; }
    operator bool() const { return _instance != nullptr; }
    
    // Allow implicit conversion to T*
    operator T*() const { return _instance; }
};

} // namespace SYSTEM

#include <esp_attr.h>

namespace SYSTEM {

/**
 * @brief Helper for managing statics allocated in EXTERNAL RAM BSS
 *        with proper alignment and lifecycle management.
 * 
 * @tparam T Service type
 */
template <typename T>
class PsramStaticService {
public:
    // Aligned buffer for placement new in PSRAM
    // Note: The instance of PsramStaticService itself must be declared with EXT_RAM_BSS_ATTR
    alignas(T) uint8_t _buffer[sizeof(T)];
    T* _instance = nullptr;

    /**
     * @brief Construct the service in the static buffer
     */
    template <typename... Args>
    T* init(Args&&... args) {
        if (_instance) {
            return _instance;
        }
        _instance = new (_buffer) T(std::forward<Args>(args)...);
        return _instance;
    }

    /**
     * @brief Manually destroy the service (call destructor)
     */
    void destroy() {
        if (_instance) {
            _instance->~T();
            _instance = nullptr;
        }
    }

    /**
     * @brief Access the service instance
     */
    T* get() const { return _instance; }
    T* operator->() const { return _instance; }
    operator bool() const { return _instance != nullptr; }
    
    // Allow implicit conversion to T*
    operator T*() const { return _instance; }
};

} // namespace SYSTEM
