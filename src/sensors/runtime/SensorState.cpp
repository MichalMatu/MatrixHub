#include "SensorState.h"
#include "SensorSnapshotHealth.h"
#include "../../config/App.h"  // TIMEOUT::* constants
#include "../../system/utils/ScopeLock.h"
#include <cstdlib>

#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "Sensor"

namespace SENSORS {

SemaphoreHandle_t SensorState::_initMutex = nullptr;
SemaphoreHandle_t SensorState::_snapshotMutex = nullptr;

SensorSnapshot SensorState::_latestSnapshot;
SensorSnapshot SensorState::_lastGoodSnapshot;
PhaseStatus SensorState::_lastReadStatus;
PhaseStatus SensorState::_lastWriteStatus;
ErrorInfo SensorState::_lastErrorInfo;

// Atomic initialization for dual-core safety
std::atomic<bool> SensorState::_initialized{false};

void SensorState::createInitMutex() {
    if (!_initMutex) {
        _initMutex = xSemaphoreCreateMutex();
        if (!_initMutex) {
            LOGE("STATE: failed to create init mutex");
            std::abort();
        }
    }
}

bool SensorState::ensureInitialized() {
    // Fast path: already initialized (atomic read - safe)
    if (_initialized.load(std::memory_order_acquire)) {
        return true;
    }

    // Verify initMutex was created in setup()
    if (!_initMutex) {
        LOGE("STATE: Init mutex is NULL! Call createInitMutex() in setup()");
        return false;
    }

    // Double-checked locking pattern for thread-safe initialization
    SYSTEM::ScopeLock lock(_initMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
    if (!lock.isLocked()) {
        LOGE("STATE: failed to acquire init mutex");
        return false;
    }

    // Second check under lock (relaxed OK - we hold the mutex)
    if (_initialized.load(std::memory_order_relaxed)) {
        return true;
    }

    if (!_snapshotMutex) {
        _snapshotMutex = xSemaphoreCreateMutex();
    }

    if (!_snapshotMutex) {
        LOGE("STATE: failed to create mutexes");
        return false;
    }

    // Store with release semantics - synchronizes with acquire above
    _initialized.store(true, std::memory_order_release);
    return true;
}

SensorSnapshot SensorState::getSnapshot() {
    SensorSnapshot snap;
    if (_snapshotMutex) {
        SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
        if (lock.isLocked()) {
            snap = _latestSnapshot;
        }
    }
    return snap;
}

SensorSnapshot SensorState::getLastGoodSnapshot() {
    SensorSnapshot snap;
    if (_snapshotMutex) {
        SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
        if (lock.isLocked()) {
            uint32_t now = millis();
            const uint32_t age = now - _lastGoodSnapshot.timestamp_ms;

            if (isSnapshotFresh(_lastGoodSnapshot.timestamp_ms, now, SENSOR::SNAPSHOT_TIMEOUT_MS)) {
                snap = _lastGoodSnapshot;
            } else {
                // Keep the retained values for diagnostics/UI context, but zero
                // the timestamp so API consumers can reliably tell the sample is
                // stale instead of silently treating old data as current.
                snap = _lastGoodSnapshot;
                snap.timestamp_ms = 0;  // Mark as invalid
                
                if (_lastGoodSnapshot.timestamp_ms != 0) {
                    LOGW("Last good snapshot is stale (age=%lu ms), marked invalid", age);
                }
            }
        }
    }
    return snap;
}

PhaseStatus SensorState::getLastReadStatus() {
    PhaseStatus status;
    if (_snapshotMutex) {
        SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
        if (lock.isLocked()) {
            status = _lastReadStatus;
        }
    }
    return status;
}

PhaseStatus SensorState::getLastWriteStatus() {
    PhaseStatus status;
    if (_snapshotMutex) {
        SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
        if (lock.isLocked()) {
            status = _lastWriteStatus;
        }
    }
    return status;
}

ErrorInfo SensorState::getLastErrorInfo() {
    ErrorInfo info;
    if (_snapshotMutex) {
        SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
        if (lock.isLocked()) {
            info = _lastErrorInfo;
        }
    }
    return info;
}

void SensorState::updateAfterRead(const SensorSnapshot& snap, const PhaseStatus& status) {
    if (!_snapshotMutex) {
        return;
    }

    SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
    if (!lock.isLocked()) {
        return;
    }

    _latestSnapshot = snap;
    _lastReadStatus = status;

    if (status.ok) {
        _lastGoodSnapshot = snap;
        _lastErrorInfo.code = nullptr;
        _lastErrorInfo.timestamp_ms = 0;
    } else if (status.error_code != nullptr) {
        _lastErrorInfo.code = status.error_code;
        _lastErrorInfo.timestamp_ms = millis();
    }
}

void SensorState::updateAfterReadFailure(const PhaseStatus& status) {
    if (!_snapshotMutex) {
        return;
    }

    SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
    if (!lock.isLocked()) {
        return;
    }

    _lastReadStatus = status;
    if (status.error_code != nullptr) {
        _lastErrorInfo.code = status.error_code;
        _lastErrorInfo.timestamp_ms = millis();
    }
}

void SensorState::updateAfterWrite(const PhaseStatus& status) {
    if (!_snapshotMutex) {
        return;
    }

    SYSTEM::ScopeLock lock(_snapshotMutex, TIMEOUT::MUTEX_STANDARD_TICKS);
    if (lock.isLocked()) {
        _lastWriteStatus = status;
    }
}

SemaphoreHandle_t SensorState::getSnapshotMutex() {
    return _snapshotMutex;
}

}  // namespace SENSORS
