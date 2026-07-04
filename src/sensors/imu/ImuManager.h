#pragma once

#include <atomic>
#include <stdint.h>
#include <cstddef>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class ImuService;

namespace IMU {

enum class Consumer : uint8_t {
    AirMouseMovement = 0,
    AirMouseClick = 1,
    AutoRotate = 2,
    Alarm = 3,
    UiMonitor = 4,
    MatrixEffects = 5
};

enum class StartError : uint8_t {
    None = 0,
    MissingBackend,
    StartFailed,
    RetryPending,
    TransitionBusy
};

struct ManagerStatus {
    uint32_t desiredMask = 0;
    uint32_t runningMask = 0;
    bool initialized = false;
    bool transitionInProgress = false;
    StartError lastStartError = StartError::None;
    uint32_t lastStartAttemptMs = 0;
    uint32_t lastStartDurationMs = 0;
    uint32_t nextRetryMs = 0;
};

class ImuManager {
public:
    struct Backend {
        std::function<bool()> start;
        std::function<void()> stop;
        std::function<bool()> isInitialized;
    };

    explicit ImuManager(ImuService* imuService);
    explicit ImuManager(Backend backend);
    ~ImuManager();

    void setConsumerActive(Consumer consumer, bool active);
    void tick();
    void clearConsumers();
    bool isActive() const {
        return _desiredMask.load(std::memory_order_acquire) != 0;
    }
    bool isRunning() const;
    uint32_t desiredMask() const { return _desiredMask.load(std::memory_order_acquire); }
    uint32_t runningMask() const { return _runningMask.load(std::memory_order_acquire); }
    ManagerStatus getStatus() const;

    static const char* consumerName(Consumer consumer);
    static const char* startErrorToString(StartError error);
    static void formatConsumers(uint32_t mask, char* out, size_t len);

private:
    ImuService* _imuService = nullptr;
    Backend _backend;
    std::atomic<uint32_t> _desiredMask{0};
    std::atomic<uint32_t> _runningMask{0};
    SemaphoreHandle_t _transitionMutex = nullptr;
    bool _transitionInProgress = false;
    StartError _lastStartError = StartError::None;
    uint32_t _lastStartAttemptMs = 0;
    uint32_t _lastStartDurationMs = 0;
    uint32_t _nextRetryMs = 0;

    static constexpr uint32_t consumerBit(Consumer consumer) {
        return 1u << static_cast<uint8_t>(consumer);
    }

    void reconcile();
    bool backendInitialized() const;
};

} // namespace IMU
