/**
 * @file RtcStatefulService.h
 * @brief StatefulService variant that stores state in RTC memory instead of DRAM
 */

#ifndef RtcStatefulService_h
#define RtcStatefulService_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <list>
#include <functional>
#include <string_view>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <core/StatefulService.h>
#include "../logging/Logging.h"
#include "RtcConfig.h"
#include "../utils/ScopeLock.h"

namespace {
    constexpr TickType_t kRtcMutexTimeout = pdMS_TO_TICKS(5000);  // service/cache mutex
    constexpr TickType_t kRtcConfigLockTimeout = pdMS_TO_TICKS(100);  // central RTC config lock
    static const char* RTC_TAG = "RtcService";
}

/**
 * StatefulService variant using RTC memory reference.
 * The state T must be a POD struct stored in RTC memory.
 *
 * The service keeps a local cached snapshot and synchronizes it through the
 * central RTC config lock before reads/after writes. This avoids holding a
 * long-lived reference into ConfigStore while keeping the IStatefulService API.
 */
template <class T>
class RtcStatefulService : public IStatefulService<T> {
public:
    /**
     * Constructor taking ConfigStore member pointer for the RTC-backed section.
     */
    explicit RtcStatefulService(T RTC::ConfigStore::* rtcMember)
        : _rtcMember(rtcMember)
        , _accessMutex(xSemaphoreCreateRecursiveMutex()) {
        syncCachedState();
    }
    
    virtual ~RtcStatefulService() {
        if (_accessMutex) {
            vSemaphoreDelete(_accessMutex);
        }
    }

    // ========================================================================
    // Update handler management
    // ========================================================================
    
    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override {
        if (!cb) return 0;
        
        update_handler_id_t id = 0;
        
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (lock.isLocked()) {
            static update_handler_id_t nextId = 1;
            id = nextId++;
            UpdateHandler handler{id, cb, allowRemove};
            _updateHandlers.push_back(handler);
        } else {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "addUpdateHandler: mutex timeout");
            return 0;
        }
        
        return id;
    }

    void removeUpdateHandler(update_handler_id_t id) override {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (lock.isLocked()) {
            _updateHandlers.remove_if([id](const UpdateHandler& h) {
                return h.allowRemove && h.id == id;
            });
        } else {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "removeUpdateHandler: mutex timeout");
        }
    }

    // ========================================================================
    // State access
    // ========================================================================
    
    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(T&)> stateUpdater, std::string_view originId) override {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "update: mutex timeout");
            return StateTransactionResult::failure("internal/update_failed");
        }
        if (!syncCachedStateLocked()) {
            return StateTransactionResult::failure("internal/update_failed");
        }

        const T previousState = _state;
        StateUpdateResult result = stateUpdater(_state);
        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
            return StateTransactionResult::failure("internal/update_failed");
        }
        if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
            _state = previousState;
            return StateTransactionResult::failure("internal/update_failed");
        }
        if (result != StateUpdateResult::CHANGED) {
            return StateTransactionResult::fromOutcome(result);
        }

        const StateHandlerResult handlerResult = callUpdateHandlers(originId);
        if (!handlerResult.ok) {
            if (!restoreState(previousState)) {
                return StateTransactionResult::failure("rtc/restore_failed");
            }
            return StateTransactionResult::failure(
                handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
                handlerResult.httpStatus);
        }

        return StateTransactionResult::fromOutcome(result);
    }

    StateUpdateResult update(std::function<StateUpdateResult(T&)> stateUpdater, std::string_view originId) override {
        return updateAndPropagate(stateUpdater, originId).outcome;
    }

    StateTransactionResult updateAndPropagate(JsonObject& jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) override {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "update(json): mutex timeout");
            return StateTransactionResult::failure("internal/update_failed");
        }
        if (!syncCachedStateLocked()) {
            return StateTransactionResult::failure("internal/update_failed");
        }

        const T previousState = _state;
        StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
            return StateTransactionResult::failure("internal/update_failed");
        }
        if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
            _state = previousState;
            return StateTransactionResult::failure("internal/update_failed");
        }
        if (result != StateUpdateResult::CHANGED) {
            return StateTransactionResult::fromOutcome(result);
        }

        const StateHandlerResult handlerResult = callUpdateHandlers(originId);
        if (!handlerResult.ok) {
            if (!restoreState(previousState)) {
                return StateTransactionResult::failure("rtc/restore_failed");
            }
            return StateTransactionResult::failure(
                handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
                handlerResult.httpStatus);
        }

        return StateTransactionResult::fromOutcome(result);
    }

    StateUpdateResult update(JsonObject& jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) override {
        return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
    }

    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(T&)> stateUpdater, std::string_view originId) override {
        (void)originId;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "updateWithoutPropagation: mutex timeout");
            return StateUpdateResult::ERROR;
        }
        if (!syncCachedStateLocked()) {
            return StateUpdateResult::ERROR;
        }
        const T previousState = _state;
        StateUpdateResult result = stateUpdater(_state);
        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
            return StateUpdateResult::ERROR;
        }
        if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
            _state = previousState;
            return StateUpdateResult::ERROR;
        }
        return result;
    }
    
    StateUpdateResult updateWithoutPropagation(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) override {
        (void)originId;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "updateWithoutPropagation(json): mutex timeout");
            return StateUpdateResult::ERROR;
        }
        if (!syncCachedStateLocked()) {
            return StateUpdateResult::ERROR;
        }
        const T previousState = _state;
        StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
            return StateUpdateResult::ERROR;
        }
        if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
            _state = previousState;
            return StateUpdateResult::ERROR;
        }
        return result;
    }

    StateHandlerResult read(std::function<void(T&)> stateReader) override {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (lock.isLocked()) {
            if (!syncCachedStateLocked()) {
                return StateHandlerResult::failure("internal/update_failed");
            }
            stateReader(_state);
            return StateHandlerResult::success();
        } else {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "read: mutex timeout");
            return StateHandlerResult::failure("internal/update_failed");
        }
    }

    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<T> stateReader) override {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (lock.isLocked()) {
            if (!syncCachedStateLocked()) {
                return StateHandlerResult::failure("internal/update_failed");
            }
            stateReader(_state, jsonObject);
            return StateHandlerResult::success();
        } else {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "read(json): mutex timeout");
            return StateHandlerResult::failure("internal/update_failed");
        }
    }

    StateHandlerResult callUpdateHandlers(std::string_view originId) override {
        std::list<UpdateHandler> handlersCopy;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (lock.isLocked()) {
            handlersCopy = _updateHandlers;
        } else {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "callUpdateHandlers: mutex timeout");
            return StateHandlerResult::failure("internal/update_failed");
        }

        for (const auto& handler : handlersCopy) {
            const StateHandlerResult handlerResult = handler.callback(originId);
            if (!handlerResult.ok) {
                return handlerResult.errorCode
                    ? handlerResult
                    : StateHandlerResult::failure("internal/update_failed", handlerResult.httpStatus);
            }
        }

        return StateHandlerResult::success();
    }

protected:
    bool syncCachedState() {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "syncCachedState: mutex timeout");
            return false;
        }
        return syncCachedStateLocked();
    }

    T getCachedStateCopy() const {
        auto* self = const_cast<RtcStatefulService*>(this);
        SYSTEM::RecursiveScopeLock lock(self->_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "getCachedStateCopy: mutex timeout");
            return T{};
        }
        if (!self->syncCachedStateLocked()) {
            return T{};
        }
        return self->_state;
    }

    T _state{};  // Local cached snapshot synchronized with RTC

private:
    struct UpdateHandler {
        update_handler_id_t id;
        StateUpdateCallback callback;
        bool allowRemove;
    };

    bool syncCachedStateLocked() {
        if (!_rtcMember) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "syncCachedStateLocked: RTC member not configured");
            return false;
        }

        SemaphoreHandle_t rtcLock = RTC::getLock();
        if (!rtcLock) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "syncCachedStateLocked: RTC config lock not initialized");
            return false;
        }

        SYSTEM::ScopeLock rtcAccess(rtcLock, kRtcConfigLockTimeout);
        if (!rtcAccess.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "syncCachedStateLocked: RTC config lock timeout");
            return false;
        }

        _state = RTC::getConfig().*_rtcMember;
        return true;
    }

    bool commitCachedStateLocked() {
        if (!_rtcMember) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "commitCachedStateLocked: RTC member not configured");
            return false;
        }

        SemaphoreHandle_t rtcLock = RTC::getLock();
        if (!rtcLock) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "commitCachedStateLocked: RTC config lock not initialized");
            return false;
        }

        SYSTEM::ScopeLock rtcAccess(rtcLock, kRtcConfigLockTimeout);
        if (!rtcAccess.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "commitCachedStateLocked: RTC config lock timeout");
            return false;
        }

        RTC::getMutableConfig().*_rtcMember = _state;
        RTC::markValid();
        return true;
    }

    bool restoreState(const T& previousState) {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, kRtcMutexTimeout);
        if (!lock.isLocked()) {
            LOG::Logging::log(ESP_LOG_WARN, RTC_TAG, "restoreState: mutex timeout");
            return false;
        }

        _state = previousState;
        return commitCachedStateLocked();
    }

    T RTC::ConfigStore::* _rtcMember;
    SemaphoreHandle_t _accessMutex;
    std::list<UpdateHandler> _updateHandlers;
};

#endif // RtcStatefulService_h
