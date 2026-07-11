#pragma once

#include <atomic>
#include <cstdint>

namespace SHELLY {

/**
 * Cross-task completion state kept separate from the FreeRTOS task handle.
 * A timed-out stop leaves the handle owned by the worker until taskEntry()
 * finishes; once finished, the suspended task is no longer reported as live
 * and a later start()/stop() can reap it safely.
 */
class ShellyWorkerLifecycleState {
public:
    void prepareStart() {
        _state.store(State::Live, std::memory_order_release);
    }

    /** Linearize a start request against taskEntry's final finish transition. */
    bool requestRestart(bool hasTaskHandle) {
        if (!hasTaskHandle) {
            return false;
        }

        State state = _state.load(std::memory_order_acquire);
        while (state != State::Finished) {
            if (state == State::RestartRequested) {
                return true;
            }
            if (_state.compare_exchange_weak(
                    state,
                    State::RestartRequested,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
        return false;
    }

    /** A newer explicit stop cancels an older request to resume the task. */
    void cancelRestartRequest() {
        State expected = State::RestartRequested;
        (void)_state.compare_exchange_strong(
            expected,
            State::Live,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    /** Called from an active task-loop iteration after observing running=true. */
    void acknowledgeLive() {
        cancelRestartRequest();
    }

    /**
     * Called exactly once after taskLoop exits. Returns true when a concurrent
     * start won and the same task must enter taskLoop again. Otherwise the task
     * is atomically published as finished and may be reaped.
     */
    bool finishOrRestart() {
        State state = _state.load(std::memory_order_acquire);
        for (;;) {
            if (state == State::RestartRequested) {
                if (_state.compare_exchange_weak(
                        state,
                        State::Live,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    return true;
                }
                continue;
            }
            if (state == State::Finished) {
                return false;
            }
            if (_state.compare_exchange_weak(
                    state,
                    State::Finished,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return false;
            }
        }
    }

    bool isLive(bool hasTaskHandle) const {
        return hasTaskHandle &&
               _state.load(std::memory_order_acquire) != State::Finished;
    }

    bool isReclaimable(bool hasTaskHandle) const {
        return hasTaskHandle &&
               _state.load(std::memory_order_acquire) == State::Finished;
    }

private:
    enum class State : uint8_t {
        Live,
        RestartRequested,
        Finished,
    };

    std::atomic<State> _state{State::Live};
};

}  // namespace SHELLY
