/**
 * @file PowerSettingsService.cpp
 * @brief Transactional settings service for power configuration
 */

#include "PowerSettingsService.h"

#include "../logging/Logging.h"
#include "../state/TransactionalStateHelpers.h"
#include "../utils/ScopeLock.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "PowerSettingsSvc"

namespace {
constexpr TickType_t kPowerSettingsMutexTimeout = pdMS_TO_TICKS(5000);

bool hasEnabledWakeSource(const RTC::PowerData& settings) {
    return settings.wakeTimerEnabled || settings.wakeButtonEnabled || settings.wakeTouchEnabled;
}

StateHandlerResult validateResolvedState(const RTC::PowerData& settings) {
    if (settings.sleepEnabled && !hasEnabledWakeSource(settings)) {
        return StateHandlerResult::failure("input/power_wake_source_required", 400);
    }
    return StateHandlerResult::success();
}
}

namespace POWER {

PowerSettingsService::PowerSettingsService(std::function<bool(const RTC::PowerData&)> applyConfig)
    : _applyConfig(std::move(applyConfig)),
      _accessMutex(xSemaphoreCreateRecursiveMutex()) {
    syncCachedStateLocked();
}

PowerSettingsService::~PowerSettingsService() {
    if (_accessMutex) {
        vSemaphoreDelete(_accessMutex);
    }
}

void PowerSettingsService::readState(RTC::PowerData& settings, JsonObject& root) {
    root[CONFIG::Keys::kSleepEnabled] = settings.sleepEnabled;
    root[CONFIG::Keys::kInactivityTimeoutMs] = settings.inactivityTimeoutMs;
    root[CONFIG::Keys::kGraceAfterBootMs] = settings.graceAfterBootMs;
    root[CONFIG::Keys::kWakeTimerEnabled] = settings.wakeTimerEnabled;
    root[CONFIG::Keys::kWakeButtonEnabled] = settings.wakeButtonEnabled;
    root[CONFIG::Keys::kWakeTouchEnabled] = settings.wakeTouchEnabled;
    root[CONFIG::Keys::kWakeIntervalMs] = settings.wakeIntervalMs;
    root[CONFIG::Keys::kTimerWakeAwakeMs] = settings.timerWakeAwakeMs;
    root[CONFIG::Keys::kButtonWakeAwakeMs] = settings.buttonWakeAwakeMs;
    root[CONFIG::Keys::kWakeTouchGpio] = settings.wakeTouchGpio;
    root[CONFIG::Keys::kWakeTouchThreshold] = settings.wakeTouchThreshold;
}

StateHandlerResult PowerSettingsService::validateStateUpdate(JsonObject& jsonObject) {
    RTC::PowerData nextState{};
    bool loaded = false;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        nextState = cfg.power;
        loaded = true;
    });

    if (!loaded) {
        return StateHandlerResult::failure("internal/update_failed");
    }

    CONFIG::JSON::deserializePower(jsonObject, nextState);
    return validateResolvedState(nextState);
}

StateUpdateResult PowerSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::PowerData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::PowerData nextState = settings;
    CONFIG::JSON::deserializePower(jsonObject, nextState);

    const StateHandlerResult validation = validateResolvedState(nextState);
    if (!validation.ok) {
        LOGW("rejecting power config: sleep_enabled requires at least one wake source");
        return StateUpdateResult::ERROR;
    }

    if (memcmp(&settings, &nextState, sizeof(RTC::PowerData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

update_handler_id_t PowerSettingsService::addUpdateHandler(StateUpdateCallback cb, bool allowRemove) {
    if (!cb) {
        return 0;
    }

    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, "addUpdateHandler")) {
        return 0;
    }

    static update_handler_id_t nextId = 1;
    const update_handler_id_t id = nextId++;
    _updateHandlers.push_back(UpdateHandler{id, cb, allowRemove});
    return id;
}

void PowerSettingsService::removeUpdateHandler(update_handler_id_t id) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, "removeUpdateHandler")) {
        return;
    }

    _updateHandlers.remove_if([id](const UpdateHandler& handler) {
        return handler.allowRemove && handler.id == id;
    });
}

StateHandlerResult PowerSettingsService::read(std::function<void(RTC::PowerData&)> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, "read")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state);
    return StateHandlerResult::success();
}

StateHandlerResult PowerSettingsService::read(JsonObject& jsonObject, JsonStateReader<RTC::PowerData> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, "read(json)")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state, jsonObject);
    return StateHandlerResult::success();
}

StateTransactionResult PowerSettingsService::updateAndPropagate(
    std::function<StateUpdateResult(RTC::PowerData&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, true, "update");
}

StateTransactionResult PowerSettingsService::updateAndPropagate(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::PowerData> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, true, "update(json)");
}

StateUpdateResult PowerSettingsService::update(
    std::function<StateUpdateResult(RTC::PowerData&)> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(stateUpdater, originId).outcome;
}

StateUpdateResult PowerSettingsService::update(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::PowerData> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
}

StateUpdateResult PowerSettingsService::updateWithoutPropagation(
    std::function<StateUpdateResult(RTC::PowerData&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, false, "updateWithoutPropagation").outcome;
}

StateUpdateResult PowerSettingsService::updateWithoutPropagation(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::PowerData> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, false, "updateWithoutPropagation(json)").outcome;
}

StateHandlerResult PowerSettingsService::callUpdateHandlers(std::string_view originId) {
    std::list<UpdateHandler> handlersCopy;
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, "callUpdateHandlers")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    handlersCopy = _updateHandlers;

    return SYSTEM::STATE::invokeUpdateHandlers(handlersCopy, originId);
}

bool PowerSettingsService::ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const {
    if (lock.isLocked()) {
        return true;
    }

    LOGW("%s: mutex timeout", operation);
    return false;
}

bool PowerSettingsService::syncCachedStateLocked() {
    bool synced = false;
    RTC::withConfig([&](const RTC::ConfigStore& cfg) {
        _state = cfg.power;
        synced = true;
    });
    if (!synced) {
        LOGW("syncCachedStateLocked: power config unavailable");
    }
    return synced;
}

bool PowerSettingsService::commitCachedStateLocked() {
    const RTC::PowerData snapshot = _state;
    if (!RTC::updateConfig([&](RTC::ConfigStore& cfg) {
            cfg.power = snapshot;
        })) {
        LOGW("commitCachedStateLocked: power config commit failed");
        return false;
    }

    return true;
}

bool PowerSettingsService::applyStateLocked(const RTC::PowerData& state) {
    if (!_applyConfig) {
        LOGW("applyStateLocked: config apply callback unavailable");
        return false;
    }

    if (!_applyConfig(state)) {
        return false;
    }

    _state = state;
    return syncCachedStateLocked();
}

bool PowerSettingsService::rollbackToState(const RTC::PowerData& state, bool propagate, const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return false;
    }

    if (propagate && _applyConfig) {
        if (applyStateLocked(state)) {
            return true;
        }
        _state = state;
        (void)commitCachedStateLocked();
        return false;
    }

    _state = state;
    return commitCachedStateLocked();
}

StateTransactionResult PowerSettingsService::runStateUpdate(
    const std::function<StateUpdateResult(RTC::PowerData&)>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateTransactionResult::failure("internal/update_failed");
    }

    const RTC::PowerData previousState = _state;
    const StateUpdateResult result = stateUpdater(_state);
    if (result == StateUpdateResult::ERROR) {
        _state = previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (result == StateUpdateResult::CHANGED) {
        const bool committed = propagate ? applyStateLocked(_state) : commitCachedStateLocked();
        if (!committed) {
            _state = previousState;
            if (!rollbackToState(previousState, propagate, "runStateUpdate/rollback")) {
                return StateTransactionResult::failure("rtc/restore_failed");
            }
            return StateTransactionResult::failure(
                propagate ? "config/apply_failed" : "internal/update_failed");
        }
    }

    return SYSTEM::STATE::finalizeStateTransaction(
        result,
        originId,
        propagate,
        [this](std::string_view handlerOriginId) {
            return callUpdateHandlers(handlerOriginId);
        },
        [this, previousState]() {
            return rollbackToState(previousState, true, "runStateUpdate/handlerRollback");
        });
}

StateTransactionResult PowerSettingsService::runJsonStateUpdate(
    JsonObject& jsonObject,
    const JsonStateUpdater<RTC::PowerData>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kPowerSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateTransactionResult::failure("internal/update_failed");
    }

    const RTC::PowerData previousState = _state;
    const StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
    if (result == StateUpdateResult::ERROR) {
        _state = previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (result == StateUpdateResult::CHANGED) {
        const bool committed = propagate ? applyStateLocked(_state) : commitCachedStateLocked();
        if (!committed) {
            _state = previousState;
            if (!rollbackToState(previousState, propagate, "runJsonStateUpdate/rollback")) {
                return StateTransactionResult::failure("rtc/restore_failed");
            }
            return StateTransactionResult::failure(
                propagate ? "config/apply_failed" : "internal/update_failed");
        }
    }

    return SYSTEM::STATE::finalizeStateTransaction(
        result,
        originId,
        propagate,
        [this](std::string_view handlerOriginId) {
            return callUpdateHandlers(handlerOriginId);
        },
        [this, previousState]() {
            return rollbackToState(previousState, true, "runJsonStateUpdate/handlerRollback");
        });
}

}  // namespace POWER
