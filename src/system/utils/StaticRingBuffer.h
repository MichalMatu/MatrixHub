/**
 * @file StaticRingBuffer.h
 * @brief Generic fixed-size circular buffer logic
 * 
 * Pure template class for ring buffer management.
 * Does not manage memory allocation (uses std::array directly).
 * Suitable for embedding in other structures (like RTC data).
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

template <typename T, size_t Capacity>
class StaticRingBuffer {
public:
    // Public state for easy embedding/serialization (e.g. in RTC struct)
    // We treat this class as a logic wrapper around this data.
    std::array<T, Capacity> buffer;
    size_t head;
    size_t count;

    // Reset state (e.g. after invalid magic)
    void clear() {
        head = 0;
        count = 0;
        // Optional: zero buffer content if T is POD
        // buffer.fill(T{}); 
    }

    // Push new item, overwriting oldest if full
    void push(const T& item) {
        buffer[head] = item;
        head = (head + 1) % Capacity;
        if (count < Capacity) {
            count++;
        }
    }

    // Current size
    size_t size() const {
        return count;
    }

    // Max capacity
    constexpr size_t capacity() const {
        return Capacity;
    }

    // Iterate over items from oldest to newest
    // Callback: void(const T&)
    template <typename Callback>
    void forEach(Callback&& cb) const {
        if (count == 0) return;

        // Start index logic:
        // If not full (count < Capacity), start is 0, oldest is at 0.
        // If full (count == Capacity), head is pointing to the *next* write slot,
        // which means the *current* slot at head is the Oldest (about to be overwritten).
        
        size_t start;
        if (count < Capacity) {
            start = 0;
        } else {
            start = head;
        }

        for (size_t i = 0; i < count; i++) {
            size_t idx = (start + i) % Capacity;
            cb(buffer[idx]);
        }
    }
    
    // Iterate over items from oldest to newest, but only the last N items
    // Efficient for "tail" operations
    template <typename Callback>
    void forEachTail(size_t maxLines, Callback&& cb) const {
        if (count == 0 || maxLines == 0) return;
        
        size_t toRead = (count < maxLines) ? count : maxLines;

        // Calculate start index for the *tail* segment
        // Logic: We want the sequence [End-N, End].
        // The "End" logical index is (Start + Count - 1).
        // The "Start" logical index is just `start` calculated above.
        // But we want to start N items before the logical end.
        
        // Let's use simplified logic:
        // Logical 0th item is always at:
        size_t logicalStart = (count < Capacity) ? 0 : head;
        
        // We want to skip (Count - N) items.
        size_t skip = count - toRead;
        
        size_t actualStart = (logicalStart + skip) % Capacity;
        
        for (size_t i = 0; i < toRead; i++) {
            size_t idx = (actualStart + i) % Capacity;
            cb(buffer[idx]);
        }
    }
};
