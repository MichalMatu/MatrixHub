#pragma once
#include <Arduino.h> // for SemaphoreHandle_t typedef
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <unordered_set>

typedef struct { int dummy; } StaticSemaphore_t;

namespace TEST_STUBS::FREERTOS {

struct BinarySemaphoreState {
    std::mutex mutex;
    std::condition_variable cv;
    bool available = false;
};

struct MutexSemaphoreState {
    std::timed_mutex mutex;
};

struct RecursiveMutexSemaphoreState {
    std::recursive_timed_mutex mutex;
};

inline TickType_t lastSemaphoreTakeTimeout = 0;
inline uint32_t semaphoreTakeCount = 0;

inline void resetSemaphoreTakeStats() {
    lastSemaphoreTakeTimeout = 0;
    semaphoreTakeCount = 0;
}

inline void recordSemaphoreTake(TickType_t timeout) {
    lastSemaphoreTakeTimeout = timeout;
    semaphoreTakeCount++;
}

inline std::unordered_set<SemaphoreHandle_t>& binarySemaphores() {
    static std::unordered_set<SemaphoreHandle_t> handles;
    return handles;
}

inline std::unordered_set<SemaphoreHandle_t>& mutexSemaphores() {
    static std::unordered_set<SemaphoreHandle_t> handles;
    return handles;
}

inline std::unordered_set<SemaphoreHandle_t>& recursiveMutexSemaphores() {
    static std::unordered_set<SemaphoreHandle_t> handles;
    return handles;
}

inline bool isBinarySemaphoreHandle(SemaphoreHandle_t handle) {
    return binarySemaphores().find(handle) != binarySemaphores().end();
}

inline bool isMutexSemaphoreHandle(SemaphoreHandle_t handle) {
    return mutexSemaphores().find(handle) != mutexSemaphores().end();
}

inline bool isRecursiveMutexSemaphoreHandle(SemaphoreHandle_t handle) {
    return recursiveMutexSemaphores().find(handle) != recursiveMutexSemaphores().end();
}

inline std::chrono::milliseconds toDuration(TickType_t ticks) {
    return std::chrono::milliseconds(ticks == portMAX_DELAY ? 0 : ticks * portTICK_PERIOD_MS);
}

inline void resetSemaphoreStubs() {
    resetSemaphoreTakeStats();
    for (SemaphoreHandle_t handle : binarySemaphores()) {
        delete static_cast<BinarySemaphoreState*>(handle);
    }
    binarySemaphores().clear();
}

}  // namespace TEST_STUBS::FREERTOS

inline SemaphoreHandle_t xSemaphoreCreateBinary() {
    auto* state = new TEST_STUBS::FREERTOS::BinarySemaphoreState();
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(state);
    TEST_STUBS::FREERTOS::binarySemaphores().insert(handle);
    return handle;
}

inline SemaphoreHandle_t xSemaphoreCreateMutex() {
    auto* state = new TEST_STUBS::FREERTOS::MutexSemaphoreState();
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(state);
    TEST_STUBS::FREERTOS::mutexSemaphores().insert(handle);
    return handle;
}

inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t* buffer) {
    (void)buffer;
    return xSemaphoreCreateMutex();
}

inline int xSemaphoreTake(SemaphoreHandle_t s, uint32_t t) {
    TEST_STUBS::FREERTOS::recordSemaphoreTake(t);
    if (!TEST_STUBS::FREERTOS::isBinarySemaphoreHandle(s)) {
        if (TEST_STUBS::FREERTOS::isMutexSemaphoreHandle(s)) {
            auto* state = static_cast<TEST_STUBS::FREERTOS::MutexSemaphoreState*>(s);
            if (t == portMAX_DELAY) {
                state->mutex.lock();
                return pdTRUE;
            }

            return state->mutex.try_lock_for(TEST_STUBS::FREERTOS::toDuration(t)) ? pdTRUE : pdFALSE;
        }

        if (TEST_STUBS::FREERTOS::isRecursiveMutexSemaphoreHandle(s)) {
            auto* state = static_cast<TEST_STUBS::FREERTOS::RecursiveMutexSemaphoreState*>(s);
            if (t == portMAX_DELAY) {
                state->mutex.lock();
                return pdTRUE;
            }

            return state->mutex.try_lock_for(TEST_STUBS::FREERTOS::toDuration(t)) ? pdTRUE : pdFALSE;
        }

        return pdFALSE;
    }

    auto* state = static_cast<TEST_STUBS::FREERTOS::BinarySemaphoreState*>(s);
    std::unique_lock<std::mutex> lock(state->mutex);
    if (!state->available) {
        if (t == portMAX_DELAY) {
            state->cv.wait(lock, [state]() { return state->available; });
        } else if (!state->cv.wait_for(lock, TEST_STUBS::FREERTOS::toDuration(t), [state]() { return state->available; })) {
            return pdFALSE;
        }
    }

    if (!state->available) {
        return pdFALSE;
    }

    state->available = false;
    return pdTRUE;
}
inline int xSemaphoreGive(SemaphoreHandle_t s) {
    if (!TEST_STUBS::FREERTOS::isBinarySemaphoreHandle(s)) {
        if (TEST_STUBS::FREERTOS::isMutexSemaphoreHandle(s)) {
            static_cast<TEST_STUBS::FREERTOS::MutexSemaphoreState*>(s)->mutex.unlock();
            return pdTRUE;
        }

        if (TEST_STUBS::FREERTOS::isRecursiveMutexSemaphoreHandle(s)) {
            static_cast<TEST_STUBS::FREERTOS::RecursiveMutexSemaphoreState*>(s)->mutex.unlock();
            return pdTRUE;
        }

        return pdFALSE;
    }

    auto* state = static_cast<TEST_STUBS::FREERTOS::BinarySemaphoreState*>(s);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->available = true;
    }
    state->cv.notify_one();
    return pdTRUE;
}
inline void vSemaphoreDelete(SemaphoreHandle_t s) {
    if (TEST_STUBS::FREERTOS::isBinarySemaphoreHandle(s)) {
        TEST_STUBS::FREERTOS::binarySemaphores().erase(s);
        delete static_cast<TEST_STUBS::FREERTOS::BinarySemaphoreState*>(s);
        return;
    }

    if (TEST_STUBS::FREERTOS::isMutexSemaphoreHandle(s)) {
        TEST_STUBS::FREERTOS::mutexSemaphores().erase(s);
        delete static_cast<TEST_STUBS::FREERTOS::MutexSemaphoreState*>(s);
        return;
    }

    if (TEST_STUBS::FREERTOS::isRecursiveMutexSemaphoreHandle(s)) {
        TEST_STUBS::FREERTOS::recursiveMutexSemaphores().erase(s);
        delete static_cast<TEST_STUBS::FREERTOS::RecursiveMutexSemaphoreState*>(s);
    }
}

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() {
    auto* state = new TEST_STUBS::FREERTOS::RecursiveMutexSemaphoreState();
    SemaphoreHandle_t handle = static_cast<SemaphoreHandle_t>(state);
    TEST_STUBS::FREERTOS::recursiveMutexSemaphores().insert(handle);
    return handle;
}

inline int xSemaphoreTakeRecursive(SemaphoreHandle_t s, uint32_t t) {
    if (!TEST_STUBS::FREERTOS::isRecursiveMutexSemaphoreHandle(s)) {
        return xSemaphoreTake(s, t);
    }

    TEST_STUBS::FREERTOS::recordSemaphoreTake(t);
    auto* state = static_cast<TEST_STUBS::FREERTOS::RecursiveMutexSemaphoreState*>(s);
    if (t == portMAX_DELAY) {
        state->mutex.lock();
        return pdTRUE;
    }

    return state->mutex.try_lock_for(TEST_STUBS::FREERTOS::toDuration(t)) ? pdTRUE : pdFALSE;
}

inline int xSemaphoreGiveRecursive(SemaphoreHandle_t s) {
    if (!TEST_STUBS::FREERTOS::isRecursiveMutexSemaphoreHandle(s)) {
        return xSemaphoreGive(s);
    }

    static_cast<TEST_STUBS::FREERTOS::RecursiveMutexSemaphoreState*>(s)->mutex.unlock();
    return pdTRUE;
}
