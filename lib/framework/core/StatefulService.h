#ifndef StatefulService_h
#define StatefulService_h

/**
 *   ESP32 SvelteKit
 *
 *   A simple, secure and extensible framework for IoT projects for ESP32 platforms
 *   with responsive Sveltekit front-end built with TailwindCSS and DaisyUI.
 *   https://github.com/theelims/ESP32-sveltekit
 *
 *   Copyright (C) 2018 - 2023 rjwats
 *   Copyright (C) 2023 - 2025 theelims
 *
 *   All Rights Reserved. This software may be modified and distributed under
 *   the terms of the LGPL v3 license. See the LICENSE file for details.
 **/

#include <Arduino.h>
#include <ArduinoJson.h>


#include <functional>
#include <string_view>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../../../src/system/utils/ScopeLock.h"

enum class StateUpdateResult
{
    CHANGED = 0, // The update changed the state and propagation should take place if required
    UNCHANGED,   // The state was unchanged, propagation should not take place
    ERROR        // There was a problem updating the state, propagation should not take place
};

struct StateHandlerResult
{
    bool ok = true;
    int httpStatus = 500;
    const char* errorCode = nullptr;

    static constexpr StateHandlerResult success()
    {
        return {};
    }

    static constexpr StateHandlerResult failure(const char* errorCode, int httpStatus = 500)
    {
        return StateHandlerResult{false, httpStatus, errorCode};
    }
};

struct StateTransactionResult
{
    StateUpdateResult outcome = StateUpdateResult::ERROR;
    int httpStatus = 500;
    const char* errorCode = nullptr;

    static constexpr StateTransactionResult fromOutcome(StateUpdateResult outcome)
    {
        return StateTransactionResult{outcome, 200, nullptr};
    }

    static constexpr StateTransactionResult failure(const char* errorCode, int httpStatus = 500)
    {
        return StateTransactionResult{StateUpdateResult::ERROR, httpStatus, errorCode};
    }
};

namespace StatefulServiceConfig {
    // Mutex timeout for all StatefulService operations (was portMAX_DELAY)
    constexpr TickType_t kMutexTimeout = pdMS_TO_TICKS(5000);
}

template <typename T>
using JsonStateUpdater = std::function<StateUpdateResult(JsonObject &root, T &settings, std::string_view originId)>;

template <typename T>
using JsonStateReader = std::function<void(T &settings, JsonObject &root)>;

typedef size_t update_handler_id_t;
typedef size_t hook_handler_id_t;
typedef std::function<StateHandlerResult(std::string_view originId)> StateUpdateCallback;
typedef std::function<void(std::string_view originId, StateUpdateResult &result)> StateHookCallback;

typedef struct StateUpdateHandlerInfo
{
    static update_handler_id_t currentUpdatedHandlerId;
    update_handler_id_t _id;
    StateUpdateCallback _cb;
    bool _allowRemove;
    StateUpdateHandlerInfo() : _id(0), _allowRemove(true) {}
    StateUpdateHandlerInfo(StateUpdateCallback cb, bool allowRemove) : _id(++currentUpdatedHandlerId), _cb(cb), _allowRemove(allowRemove) {};
} StateUpdateHandlerInfo_t;

typedef struct StateHookHandlerInfo
{
    static hook_handler_id_t currentHookHandlerId;
    hook_handler_id_t _id;
    StateHookCallback _cb;
    bool _allowRemove;
    StateHookHandlerInfo() : _id(0), _allowRemove(true) {}
    StateHookHandlerInfo(StateHookCallback cb, bool allowRemove) : _id(++currentHookHandlerId), _cb(cb), _allowRemove(allowRemove) {};
} StateHookHandlerInfo_t;

#define MAX_STATE_HANDLERS 4

/**
 * Interface for Stateful Services.
 * Allows substitution of implementation (e.g. DRAM vs RTC backed).
 */
template <class T>
class IStatefulService {
public:
    virtual ~IStatefulService() = default;

    virtual update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) = 0;
    virtual void removeUpdateHandler(update_handler_id_t id) = 0;
    
    // Read methods
    virtual StateHandlerResult read(std::function<void(T &)> stateReader) = 0;
    virtual StateHandlerResult read(JsonObject &jsonObject, JsonStateReader<T> stateReader) = 0;
    
    // Update methods
    virtual StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(T &)> stateUpdater, std::string_view originId) = 0;
    virtual StateTransactionResult updateAndPropagate(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) = 0;
    virtual StateUpdateResult update(std::function<StateUpdateResult(T &)> stateUpdater, std::string_view originId) = 0;
    virtual StateUpdateResult update(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) = 0;
    
    virtual StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(T &)> stateUpdater, std::string_view originId) = 0;
    virtual StateUpdateResult updateWithoutPropagation(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) = 0;
    
    virtual StateHandlerResult callUpdateHandlers(std::string_view originId) = 0;
};

template <class T>
class StatefulService : public IStatefulService<T>
{
public:
    template <typename... Args>
    StatefulService(Args &&...args) : _state(std::forward<Args>(args)...), _accessMutex(xSemaphoreCreateRecursiveMutex())
    {
    }

    virtual ~StatefulService()
    {
        if (_accessMutex) {
            vSemaphoreDelete(_accessMutex);
        }
    }

    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override
    {
        if (!cb) return 0;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return 0;
        for(int i=0; i<MAX_STATE_HANDLERS; i++) {
            if(_updateHandlers[i]._id == 0) {
                _updateHandlers[i] = StateUpdateHandlerInfo_t(cb, allowRemove);
                update_handler_id_t id = _updateHandlers[i]._id;
                return id;
            }
        }
        return 0; // Full
    }

    void removeUpdateHandler(update_handler_id_t id) override
    {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return;
        for(int i=0; i<MAX_STATE_HANDLERS; i++) {
            if(_updateHandlers[i]._id == id && _updateHandlers[i]._allowRemove) {
                _updateHandlers[i]._id = 0; // Mark as free
                _updateHandlers[i]._cb = nullptr;
                break;
            }
        }
    }

    hook_handler_id_t addHookHandler(StateHookCallback cb, bool allowRemove = true)
    {
        if (!cb) return 0;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return 0;
        for(int i=0; i<MAX_STATE_HANDLERS; i++) {
            if(_hookHandlers[i]._id == 0) {
                _hookHandlers[i] = StateHookHandlerInfo_t(cb, allowRemove);
                hook_handler_id_t id = _hookHandlers[i]._id;
                return id;
            }
        }
        return 0;
    }

    void removeHookHandler(hook_handler_id_t id)
    {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return;
        for(int i=0; i<MAX_STATE_HANDLERS; i++) {
            if(_hookHandlers[i]._id == id && _hookHandlers[i]._allowRemove) {
                _hookHandlers[i]._id = 0;
                _hookHandlers[i]._cb = nullptr;
                break;
            }
        }
    }

    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(T &)> stateUpdater, std::string_view originId) override
    {
        // Intentionally keep the service mutex held for the whole update+handler sequence.
        // This makes the transaction conservative: other readers/writers cannot observe
        // partially applied state or race with rollback while update handlers persist/apply it.
        // The trade-off is that slow handlers block subsequent updates, so revisit this only
        // together with a broader transaction redesign and matching test updates.
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) {
            return StateTransactionResult::failure("internal/update_failed");
        }

        const T previousState = _state;
        StateUpdateResult result = stateUpdater(_state);
        callHookHandlers(originId, result);

        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
            return StateTransactionResult::failure("internal/update_failed");
        }

        if (result == StateUpdateResult::CHANGED) {
            const StateHandlerResult handlerResult = callUpdateHandlers(originId);
            if (!handlerResult.ok) {
                _state = previousState;
                return StateTransactionResult::failure(
                    handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
                    handlerResult.httpStatus);
            }
        }

        return StateTransactionResult::fromOutcome(result);
    }

    StateUpdateResult update(std::function<StateUpdateResult(T &)> stateUpdater, std::string_view originId) override
    {
        return updateAndPropagate(stateUpdater, originId).outcome;
    }

    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(T &)> stateUpdater, std::string_view originId) override
    {
        T previousState{};
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return StateUpdateResult::ERROR;
        previousState = _state;
        StateUpdateResult result = stateUpdater(_state);
        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
        }
        return result;
    }

    StateTransactionResult updateAndPropagate(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) override
    {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) {
            return StateTransactionResult::failure("internal/update_failed");
        }

        const T previousState = _state;
        StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
        callHookHandlers(originId, result);

        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
            return StateTransactionResult::failure("internal/update_failed");
        }

        if (result == StateUpdateResult::CHANGED) {
            const StateHandlerResult handlerResult = callUpdateHandlers(originId);
            if (!handlerResult.ok) {
                _state = previousState;
                return StateTransactionResult::failure(
                    handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
                    handlerResult.httpStatus);
            }
        }

        return StateTransactionResult::fromOutcome(result);
    }

    StateUpdateResult update(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) override
    {
        return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
    }

    StateUpdateResult updateWithoutPropagation(JsonObject &jsonObject, JsonStateUpdater<T> stateUpdater, std::string_view originId) override
    {
        T previousState{};
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return StateUpdateResult::ERROR;
        previousState = _state;
        StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
        if (result == StateUpdateResult::ERROR) {
            _state = previousState;
        }
        return result;
    }

    StateHandlerResult read(std::function<void(T &)> stateReader) override
    {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) {
            return StateHandlerResult::failure("internal/update_failed");
        }
        stateReader(_state);
        return StateHandlerResult::success();
    }

    StateHandlerResult read(JsonObject &jsonObject, JsonStateReader<T> stateReader) override
    {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) {
            return StateHandlerResult::failure("internal/update_failed");
        }
        stateReader(_state, jsonObject);
        return StateHandlerResult::success();
    }

    StateHandlerResult callUpdateHandlers(std::string_view originId) override
    {
        StateUpdateHandlerInfo_t handlersCopy[MAX_STATE_HANDLERS];
        int count = 0;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) {
            return StateHandlerResult::failure("internal/update_failed");
        }
        for(int i=0; i<MAX_STATE_HANDLERS; i++) {
            if(_updateHandlers[i]._id != 0) {
                handlersCopy[count++] = _updateHandlers[i];
            }
        }

        for(int i=0; i<count; i++) {
            if (!handlersCopy[i]._cb) {
                continue;
            }

            const StateHandlerResult handlerResult = handlersCopy[i]._cb(originId);
            if (!handlerResult.ok) {
                return handlerResult.errorCode
                    ? handlerResult
                    : StateHandlerResult::failure("internal/update_failed", handlerResult.httpStatus);
            }
        }

        return StateHandlerResult::success();
    }

    void callHookHandlers(std::string_view originId, StateUpdateResult &result)
    {
        StateHookHandlerInfo_t handlersCopy[MAX_STATE_HANDLERS];
        int count = 0;
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) return;
        for(int i=0; i<MAX_STATE_HANDLERS; i++) {
            if(_hookHandlers[i]._id != 0) {
                handlersCopy[count++] = _hookHandlers[i];
            }
        }

        for(int i=0; i<count; i++) {
            if(handlersCopy[i]._cb) handlersCopy[i]._cb(originId, result);
        }
    }

 protected:
    T _state;

 private:
    bool restoreState(const T& previousState)
    {
        SYSTEM::RecursiveScopeLock lock(_accessMutex, StatefulServiceConfig::kMutexTimeout);
        if (!lock.isLocked()) {
            return false;
        }
        _state = previousState;
        return true;
    }

    SemaphoreHandle_t _accessMutex;
    StateUpdateHandlerInfo_t _updateHandlers[MAX_STATE_HANDLERS];
    StateHookHandlerInfo_t _hookHandlers[MAX_STATE_HANDLERS];
};

#endif // end StatefulService_h
