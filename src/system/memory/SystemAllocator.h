#pragma once

#include <cstdint>
#include <memory>
#include <new>
#include <utility>
#include <esp_heap_caps.h>
#include <esp_log.h>

namespace SYSTEM {
namespace MEMORY {

    /**
     * @brief Allocates an object in PSRAM only.
     *        Uses placement new to construct the object.
     * 
     * @tparam T Type of object to allocate
     * @tparam Args Constructor arguments
     * @return T* Pointer to the constructed object or nullptr on failure
     */
    template <typename T, typename... Args>
    T* allocInPsram(Args&&... args) {
        // PSRAM-only allocation (8-bit accessible capabilities)
        void* ptr = heap_caps_malloc(sizeof(T), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (!ptr) {
             ESP_LOGE("SystemAllocator", "Critical: PSRAM allocation failed for %lu bytes",
                      static_cast<unsigned long>(sizeof(T)));
             return nullptr;
        }

        // Placement new
        return new (ptr) T(std::forward<Args>(args)...);
    }

    template <typename T>
    struct PsramDeleter {
        void operator()(T* ptr) const noexcept {
            if (!ptr) {
                return;
            }

            ptr->~T();
            heap_caps_free(ptr);
        }
    };

    template <typename T>
    using PsramUniquePtr = std::unique_ptr<T, PsramDeleter<T>>;

    /**
     * @brief Allocates a temporary CPU-owned object in PSRAM.
     *
     * Before the alarm refactor, several code paths built multi-kilobyte
     * snapshots as local stack objects. This helper keeps those transient
     * snapshots off task stacks while preserving normal RAII cleanup.
     *
     * Large config snapshots are safe here because they are plain CPU data,
     * not DMA buffers, ISR state, FreeRTOS stacks, or LwIP task stacks.
     */
    template <typename T, typename... Args>
    PsramUniquePtr<T> makeUniqueInPsram(Args&&... args) {
        return PsramUniquePtr<T>(allocInPsram<T>(std::forward<Args>(args)...));
    }

} // namespace MEMORY
} // namespace SYSTEM
