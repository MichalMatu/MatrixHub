/**
 * @file LogRingBuffer.h
 * @brief Thread-safe ring buffer for in-memory log storage
 * 
 * Provides a fixed-size circular buffer for storing recent log entries
 * with mutex protection for concurrent access. Designed for ESP32 DRAM
 * constraints - avoids dynamic allocations in hot path.
 */

#ifndef LogRingBuffer_h
#define LogRingBuffer_h

#include <Arduino.h>
#include <array>
#include <vector>
#include <utility>
#include "../../config/App.h"
#include "../../config/TaskConfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "../../system/utils/StaticRingBuffer.h"
#include "../../system/utils/ScopeLock.h"

namespace LOG {

/**
 * Single log entry (POD struct, no String to avoid fragmentation).
 * Fixed-size fields keep memory predictable across buffer resizes.
 */
struct Line {
    uint32_t timestampMs{0};
    char levelChar{'N'};           // E/W/I/D/V/N
    char tag[12]{};                // Tag (e.g. "Sensor", "WiFi")
    char message[96]{};            // Message buffer
};

/**
 * Thread-safe circular buffer for log lines.
 * Uses FreeRTOS mutex for concurrent access protection.
 * Backed by PSRAM to avoid pressure on internal DRAM.
 */
class RingBuffer {
public:
    static constexpr size_t kFixedCapacity = LOG_CFG::RING_BUFFER_CAPACITY;

    /**
     * Initialize ring buffer with mutex.
     * Must be called once during system initialization.
     */
    static void begin(uint16_t capacity);

    /**
     * Release all allocated resources (PSRAM and mutext).
     */
    static void end();

    /**
     * Append a new log line to the ring buffer.
     * Thread-safe. Non-blocking with short timeout.
     */
    static void append(const char *levelLabel, const char *tag, const char *message);

    /**
     * Iterate over last N lines without heap allocation.
     * Thread-safe snapshot-like iteration.
     */
    template <typename Callback>
    static void forEachTail(size_t maxLines, Callback&& callback) {
        if (!_mutex || maxLines == 0) {
            return;
        }

        SYSTEM::ScopeLock lock(_mutex, pdMS_TO_TICKS(LOG_CFG::RING_SNAPSHOT_LOCK_MS));
        if (!lock.isLocked()) {
            return;
        }
        
        // Delegate to static buffer logic
        if (_store) {
            _store->forEachTail(maxLines, std::forward<Callback>(callback));
        }
    }

    /**
     * Clear all entries in the ring buffer.
     */
    static void clear();

    /**
     * Copy last N lines to caller buffer (oldest->newest order).
     * The copy is done under mutex, then caller can process without holding lock.
     *
     * @return Number of copied lines (0 on empty/error).
     */
    static size_t copyTail(Line* out, size_t maxLines);

    /**
     * Copy a window from the tail (oldest->newest order).
     *
     * @param out Destination buffer.
     * @param tailMaxLines Tail size to consider (e.g., requested "lines" from API).
     * @param offsetFromOldest Offset inside that tail window.
     * @param maxLines Max lines to copy into out.
     * @return Number of copied lines (0 on empty/error/end).
     */
    static size_t copyTailRange(Line* out, size_t tailMaxLines, size_t offsetFromOldest, size_t maxLines);

    static size_t count() { return _store ? _store->size() : 0; }
    static size_t capacity() { return _store ? _store->capacity() : 0; }
    static size_t getPsramMemoryUsage() { return _store ? sizeof(*_store) : 0; }

private:
    // Internal generic store - located in PSRAM (allocated dynamically)
    static StaticRingBuffer<Line, kFixedCapacity>* _store;

    static SemaphoreHandle_t _mutex;   // DRAM - recreated on boot

    RingBuffer() = delete;
};

}  // namespace LOG

#endif
