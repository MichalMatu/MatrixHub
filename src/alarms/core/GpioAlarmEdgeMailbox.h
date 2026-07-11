#pragma once

#include "CsiAlarmEdgeLatch.h"
#include "../types/AlarmConstants.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ALARMS {

/**
 * Fixed-size, allocation-free mailbox for debounced GPIO logical edges.
 *
 * GPIO values are selector-scoped: an edge for `gpio1` must never override a
 * rule bound to `gpio2`.  A rising edge is retained ahead of a later clear so
 * a complete pulse cannot disappear while the main alarm loop is busy.  Passes
 * are only acknowledged after AlarmCoordinator commits its runtime state.
 */
class GpioAlarmEdgeMailbox {
public:
    using PendingDecision = BooleanAlarmEdgeLatch::PendingDecision;

    // Eight channels may be replaced by another eight in one config update.
    // Keeping both sets lets the old selectors publish their terminal clear
    // without allocating or dropping the new selectors' initial levels.
    static constexpr uint8_t kMaxEntries = kMaxRules * 2;

    struct PendingEdge {
        char gpioId[kGpioIdLen]{};
        PendingDecision decision = PendingDecision::None;
    };

    struct PassSnapshot {
        PendingEdge edges[kMaxEntries]{};
        uint8_t count = 0;

        bool valueFor(const char* gpioId, bool& value) const {
            if (!gpioId || gpioId[0] == '\0') {
                return false;
            }
            for (uint8_t i = 0; i < count; ++i) {
                if (std::strncmp(edges[i].gpioId, gpioId, kGpioIdLen) != 0) {
                    continue;
                }
                if (edges[i].decision == PendingDecision::Rising) {
                    value = true;
                    return true;
                }
                if (edges[i].decision == PendingDecision::Clear) {
                    value = false;
                    return true;
                }
                return false;
            }
            return false;
        }
    };

    bool submit(const char* gpioId, bool logicalValue) {
        const size_t length = boundedLength(gpioId);
        if (length == 0 || length >= kGpioIdLen) {
            return false;
        }

        Entry* entry = find(gpioId);
        if (!entry) {
            entry = allocateEntry();
            if (!entry) {
                return false;
            }
            std::memcpy(entry->gpioId, gpioId, length + 1);
        }

        entry->latch.submit(logicalValue);
        entry->latestValue = logicalValue;
        return true;
    }

    bool peek(PassSnapshot& snapshot) const {
        snapshot = PassSnapshot{};
        for (uint8_t i = 0; i < kMaxEntries; ++i) {
            if (!_entries[i].used || !_entries[i].latch.hasPending()) {
                continue;
            }

            PendingEdge& edge = snapshot.edges[snapshot.count++];
            std::memcpy(edge.gpioId, _entries[i].gpioId, kGpioIdLen);
            edge.decision = _entries[i].latch.next();
        }
        return snapshot.count > 0;
    }

    void complete(const PassSnapshot& snapshot) {
        for (uint8_t i = 0; i < snapshot.count; ++i) {
            Entry* entry = find(snapshot.edges[i].gpioId);
            if (entry) {
                entry->latch.complete(snapshot.edges[i].decision);
            }
        }
    }

    bool hasPending() const {
        for (uint8_t i = 0; i < kMaxEntries; ++i) {
            if (_entries[i].used && _entries[i].latch.hasPending()) {
                return true;
            }
        }
        return false;
    }

private:
    struct Entry {
        char gpioId[kGpioIdLen]{};
        BooleanAlarmEdgeLatch latch;
        bool used = false;
        bool latestValue = false;
    };

    static size_t boundedLength(const char* value) {
        if (!value) {
            return kGpioIdLen;
        }
        size_t length = 0;
        while (length < kGpioIdLen && value[length] != '\0') {
            ++length;
        }
        return length;
    }

    Entry* find(const char* gpioId) {
        for (uint8_t i = 0; i < kMaxEntries; ++i) {
            if (_entries[i].used &&
                std::strncmp(_entries[i].gpioId, gpioId, kGpioIdLen) == 0) {
                return &_entries[i];
            }
        }
        return nullptr;
    }

    Entry* allocateEntry() {
        for (uint8_t i = 0; i < kMaxEntries; ++i) {
            if (!_entries[i].used) {
                _entries[i] = Entry{};
                _entries[i].used = true;
                return &_entries[i];
            }
        }

        // A settled low selector is safe to recycle: if it becomes active
        // again, its first definitive low still only initializes/clears state.
        for (uint8_t i = 0; i < kMaxEntries; ++i) {
            if (!_entries[i].latch.hasPending() && !_entries[i].latestValue) {
                _entries[i] = Entry{};
                _entries[i].used = true;
                return &_entries[i];
            }
        }
        return nullptr;
    }

    Entry _entries[kMaxEntries]{};
};

}  // namespace ALARMS
