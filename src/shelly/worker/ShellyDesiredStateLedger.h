#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "../ShellyConfig.h"

namespace SHELLY {

struct ShellyDesiredStateLease {
    char id[kMaxShellyId];
    bool value;
    uint32_t generation;
    uint64_t peerRevision;

    ShellyDesiredStateLease() : value(false), generation(0), peerRevision(0) {
        id[0] = '\0';
    }
};

enum class ShellyDesiredStateCompletion : uint8_t {
    Ignored,
    Applied,
    Superseded,
    RetryScheduled,
};

/**
 * Fixed-size, allocation-free source of truth for relay intent.
 *
 * The FreeRTOS queue only wakes the worker. Keeping the latest desired state
 * here means a saturated wake queue cannot discard a final OFF command.
 * Synchronization is intentionally owned by ShellyWorker so this class stays
 * deterministic and host-testable.
 */
class ShellyDesiredStateLedger {
public:
    // Compatibility convenience for callers/tests that do not model a peer.
    bool upsert(const char* id,
                bool value,
                uint32_t nowMs,
                uint32_t* generationOut = nullptr) {
        return upsert(id, value, 0, nowMs, generationOut);
    }

    bool upsert(const char* id,
                bool value,
                uint64_t peerRevision,
                uint32_t nowMs,
                uint32_t* generationOut = nullptr) {
        const size_t idLength = boundedIdLength(id);
        if (idLength == 0 || idLength >= kMaxShellyId) {
            return false;
        }

        Slot* slot = find(id);
        if (slot) {
            if (slot->value == value && slot->peerRevision == peerRevision &&
                (slot->pending || slot->inFlight)) {
                if (generationOut) {
                    *generationOut = slot->generation;
                }
                return true;
            }
        } else {
            slot = findAvailableSlot();
            if (!slot) {
                return false;
            }

            *slot = Slot{};
            slot->used = true;
            std::memcpy(slot->id, id, idLength + 1);
        }

        slot->value = value;
        slot->peerRevision = peerRevision;
        slot->generation = nextGeneration();
        slot->pending = true;
        slot->retryCount = 0;
        slot->nextAttemptMs = nowMs;

        if (generationOut) {
            *generationOut = slot->generation;
        }
        return true;
    }

    /**
     * Preserve the latest desired value while moving it to a changed peer.
     * Returns false when this device has no retained intent to migrate.
     */
    bool rebind(const char* id, uint64_t peerRevision, uint32_t nowMs) {
        Slot* slot = find(id);
        if (!slot) {
            return false;
        }
        if (slot->peerRevision == peerRevision) {
            return true;
        }

        slot->peerRevision = peerRevision;
        slot->generation = nextGeneration();
        slot->pending = true;
        slot->retryCount = 0;
        slot->nextAttemptMs = nowMs;
        return true;
    }

    /**
     * Rebind only the generation represented by an in-flight lease. A newer
     * command may have arrived after the device lookup; that newer generation
     * owns its peer revision and must never be overwritten by the old worker.
     */
    bool rebindIfGeneration(const char* id,
                            uint32_t expectedGeneration,
                            uint64_t peerRevision,
                            uint32_t nowMs) {
        Slot* slot = find(id);
        if (!slot || slot->generation != expectedGeneration) {
            return false;
        }
        return rebind(id, peerRevision, nowMs);
    }

    /** Retain the latest value but make it ineligible while a peer is disabled. */
    bool park(const char* id, uint64_t peerRevision) {
        Slot* slot = find(id);
        if (!slot) {
            return false;
        }

        slot->peerRevision = peerRevision;
        slot->generation = nextGeneration();
        slot->pending = false;
        slot->inFlight = false;
        slot->inFlightGeneration = 0;
        slot->retryCount = 0;
        slot->nextAttemptMs = 0;
        return true;
    }

    bool parkIfGeneration(const char* id,
                          uint32_t expectedGeneration,
                          uint64_t peerRevision) {
        Slot* slot = find(id);
        if (!slot || slot->generation != expectedGeneration) {
            return false;
        }
        return park(id, peerRevision);
    }

    bool beginNextReady(uint32_t nowMs, ShellyDesiredStateLease& out) {
        for (size_t offset = 0; offset < kMaxDevices; ++offset) {
            const size_t index = (_nextScanIndex + offset) % kMaxDevices;
            Slot& slot = _slots[index];
            if (!slot.used || !slot.pending || slot.inFlight ||
                !deadlineReached(nowMs, slot.nextAttemptMs)) {
                continue;
            }

            slot.inFlight = true;
            slot.inFlightGeneration = slot.generation;
            std::memcpy(out.id, slot.id, sizeof(out.id));
            out.value = slot.value;
            out.generation = slot.generation;
            out.peerRevision = slot.peerRevision;
            _nextScanIndex = (index + 1) % kMaxDevices;
            return true;
        }
        return false;
    }

    ShellyDesiredStateCompletion complete(const char* id,
                                           uint32_t generation,
                                           bool success,
                                           uint32_t nowMs) {
        Slot* slot = find(id);
        if (!slot || !slot->inFlight || slot->inFlightGeneration != generation) {
            return ShellyDesiredStateCompletion::Ignored;
        }

        slot->inFlight = false;
        slot->inFlightGeneration = 0;

        if (slot->generation != generation) {
            // A newer desired state arrived while transport was in flight.
            // Its attempt must be immediately eligible regardless of this ACK.
            slot->pending = true;
            slot->nextAttemptMs = nowMs;
            return ShellyDesiredStateCompletion::Superseded;
        }

        if (success) {
            slot->pending = false;
            slot->retryCount = 0;
            slot->nextAttemptMs = 0;
            return ShellyDesiredStateCompletion::Applied;
        }

        if (slot->retryCount < UINT8_MAX) {
            ++slot->retryCount;
        }
        slot->pending = true;
        slot->nextAttemptMs = nowMs + retryDelayMs(slot->retryCount);
        return ShellyDesiredStateCompletion::RetryScheduled;
    }

    bool remove(const char* id) {
        Slot* slot = find(id);
        if (!slot) {
            return false;
        }
        *slot = Slot{};
        return true;
    }


    /** Remove a definitively missing/disabled peer without deleting newer intent. */
    bool removeIfGeneration(const char* id, uint32_t expectedGeneration) {
        Slot* slot = find(id);
        if (!slot || slot->generation != expectedGeneration) {
            return false;
        }
        *slot = Slot{};
        return true;
    }

    size_t pendingCount() const {
        size_t count = 0;
        for (const Slot& slot : _slots) {
            if (slot.used && slot.pending) {
                ++count;
            }
        }
        return count;
    }

    static uint32_t retryDelayMs(uint8_t consecutiveFailures) {
        if (consecutiveFailures == 0) {
            return 0;
        }

        // Keep the existing three-attempt fast recovery window, then switch
        // to a low-frequency retry cadence without ever forgetting intent.
        if (consecutiveFailures > kHttpMaxRetries) {
            return kHttpRecoveryRetryDelayMs;
        }

        return static_cast<uint32_t>(kHttpRetryDelayMs)
               << (consecutiveFailures - 1);
    }

private:
    struct Slot {
        bool used = false;
        char id[kMaxShellyId] = {0};
        bool value = false;
        uint64_t peerRevision = 0;
        bool pending = false;
        bool inFlight = false;
        uint8_t retryCount = 0;
        uint32_t generation = 0;
        uint32_t inFlightGeneration = 0;
        uint32_t nextAttemptMs = 0;
    };

    static size_t boundedIdLength(const char* id) {
        if (!id) {
            return 0;
        }
        size_t length = 0;
        while (length < kMaxShellyId && id[length] != '\0') {
            ++length;
        }
        return length;
    }

    static bool deadlineReached(uint32_t nowMs, uint32_t deadlineMs) {
        return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
    }

    Slot* find(const char* id) {
        if (!id) {
            return nullptr;
        }
        for (Slot& slot : _slots) {
            if (slot.used && std::strncmp(slot.id, id, sizeof(slot.id)) == 0) {
                return &slot;
            }
        }
        return nullptr;
    }

    Slot* findAvailableSlot() {
        Slot* idle = nullptr;
        for (Slot& slot : _slots) {
            if (!slot.used) {
                return &slot;
            }
            if (!idle && !slot.pending && !slot.inFlight) {
                idle = &slot;
            }
        }
        return idle;
    }

    uint32_t nextGeneration() {
        ++_nextGeneration;
        if (_nextGeneration == 0) {
            ++_nextGeneration;
        }
        return _nextGeneration;
    }

    Slot _slots[kMaxDevices]{};
    size_t _nextScanIndex = 0;
    uint32_t _nextGeneration = 0;
};

}  // namespace SHELLY
