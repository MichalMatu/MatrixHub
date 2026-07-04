/**
 * @file AlarmSettingsService.cpp
 * @brief Transactional settings service for alarm rules
 */

#include "AlarmSettingsService.h"

#include "AlarmSettingsPayload.h"
#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"
#include "../system/memory/SystemAllocator.h"
#include "../system/state/TransactionalStateHelpers.h"
#include "../system/utils/ScopeLock.h"
#include "AlarmRulesStore.h"

#undef LOG_TAG
#define LOG_TAG "AlarmSettings"

namespace {
constexpr TickType_t kAlarmSettingsMutexTimeout = pdMS_TO_TICKS(5000);
}  // namespace

namespace ALARMS {

AlarmSettingsService::AlarmSettingsService(
    FS* fs,
    std::function<bool(const AlarmRulesSnapshot&)> applyRules)
    : _fs(fs),
      _applyRules(std::move(applyRules)),
      _accessMutex(xSemaphoreCreateRecursiveMutex()) {
    syncCachedStateLocked();

    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistAndApply();
        },
        false);
}

AlarmSettingsService::~AlarmSettingsService() {
    if (_accessMutex) {
        vSemaphoreDelete(_accessMutex);
    }
}

void AlarmSettingsService::readState(AlarmRulesSnapshot& settings, JsonObject& root) {
    AlarmSettingsPayload::readState(settings, root);
}

StateUpdateResult AlarmSettingsService::updateState(
    JsonObject& jsonObject,
    AlarmRulesSnapshot& settings,
    std::string_view originId) {
    (void)originId;

    return AlarmSettingsPayload::updateState(jsonObject, settings, originId);
}

StateHandlerResult AlarmSettingsService::validateStateUpdate(JsonObject& jsonObject) {
    return AlarmSettingsPayload::parseUpdate(jsonObject, nullptr);
}

update_handler_id_t AlarmSettingsService::addUpdateHandler(StateUpdateCallback cb, bool allowRemove) {
    if (!cb) {
        return 0;
    }

    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, "addUpdateHandler")) {
        return 0;
    }

    static update_handler_id_t nextId = 1;
    const update_handler_id_t id = nextId++;
    _updateHandlers.push_back(UpdateHandler{id, cb, allowRemove});
    return id;
}

void AlarmSettingsService::removeUpdateHandler(update_handler_id_t id) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, "removeUpdateHandler")) {
        return;
    }

    _updateHandlers.remove_if([id](const UpdateHandler& handler) {
        return handler.allowRemove && handler.id == id;
    });
}

StateHandlerResult AlarmSettingsService::read(std::function<void(AlarmRulesSnapshot&)> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, "read")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state);
    return StateHandlerResult::success();
}

StateHandlerResult AlarmSettingsService::read(JsonObject& jsonObject, JsonStateReader<AlarmRulesSnapshot> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, "read(json)")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state, jsonObject);
    return StateHandlerResult::success();
}

StateTransactionResult AlarmSettingsService::updateAndPropagate(
    std::function<StateUpdateResult(AlarmRulesSnapshot&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, true, "update");
}

StateTransactionResult AlarmSettingsService::updateAndPropagate(
    JsonObject& jsonObject,
    JsonStateUpdater<AlarmRulesSnapshot> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, true, "update(json)");
}

StateUpdateResult AlarmSettingsService::update(
    std::function<StateUpdateResult(AlarmRulesSnapshot&)> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(stateUpdater, originId).outcome;
}

StateUpdateResult AlarmSettingsService::update(
    JsonObject& jsonObject,
    JsonStateUpdater<AlarmRulesSnapshot> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
}

StateUpdateResult AlarmSettingsService::updateWithoutPropagation(
    std::function<StateUpdateResult(AlarmRulesSnapshot&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, false, "updateWithoutPropagation").outcome;
}

StateUpdateResult AlarmSettingsService::updateWithoutPropagation(
    JsonObject& jsonObject,
    JsonStateUpdater<AlarmRulesSnapshot> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, false, "updateWithoutPropagation(json)").outcome;
}

StateHandlerResult AlarmSettingsService::callUpdateHandlers(std::string_view originId) {
    std::list<UpdateHandler> handlersCopy;
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, "callUpdateHandlers")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    handlersCopy = _updateHandlers;

    return SYSTEM::STATE::invokeUpdateHandlers(handlersCopy, originId);
}

bool AlarmSettingsService::ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const {
    if (lock.isLocked()) {
        return true;
    }

    LOGW("%s: mutex timeout", operation);
    return false;
}

bool AlarmSettingsService::syncCachedStateLocked() {
    bool synced = false;
    ALARMS::RULES_CONFIG::withRules([&](const AlarmRulesSnapshot& state) {
        _state = state;
        synced = true;
    });
    if (!synced) {
        LOGW("syncCachedStateLocked: alarm rules unavailable");
    }
    return synced;
}

bool AlarmSettingsService::commitCachedStateLocked() {
    if (!ALARMS::RULES_CONFIG::update([&](AlarmRulesSnapshot& state) {
            state = _state;
        })) {
        LOGW("commitCachedStateLocked: alarm rules commit failed");
        return false;
    }

    return true;
}

bool AlarmSettingsService::persistStateSnapshot(const AlarmRulesSnapshot& state) const {
    if (!_fs) {
        LOGW("persistStateSnapshot: filesystem unavailable");
        return false;
    }

    const bool saved = CONFIG::saveWithAlarmRules(*_fs, state);
    if (!saved) {
        LOGW("persistStateSnapshot: saveWithAlarmRules failed");
    }
    return saved;
}

bool AlarmSettingsService::applyStateSnapshot(const AlarmRulesSnapshot& state) const {
    if (!_applyRules) {
        LOGW("applyStateSnapshot: apply callback unavailable");
        return false;
    }

    const bool applied = _applyRules(state);
    if (!applied) {
        LOGW("applyStateSnapshot: runtime apply callback failed");
    }
    return applied;
}

bool AlarmSettingsService::rollbackToState(
    const AlarmRulesSnapshot& state,
    bool propagate,
    const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return false;
    }

    _state = state;
    if (!commitCachedStateLocked()) {
        return false;
    }

    if (!propagate) {
        return true;
    }

    if (!persistStateSnapshot(state)) {
        return false;
    }

    return applyStateSnapshot(state);
}

StateHandlerResult AlarmSettingsService::persistAndApply() {
    if (!persistStateSnapshot(_state)) {
        LOGE("Failed to persist alarm rules");
        return StateHandlerResult::failure("config/save_failed");
    }

    if (!applyStateSnapshot(_state)) {
        LOGE("Failed to apply alarm rules");
        return StateHandlerResult::failure("config/apply_failed");
    }

    return StateHandlerResult::success();
}

StateTransactionResult AlarmSettingsService::runStateUpdate(
    const std::function<StateUpdateResult(AlarmRulesSnapshot&)>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<AlarmRulesSnapshot>();
    if (!previousState) {
        LOGE("%s: failed to allocate previous alarm state in PSRAM", operation);
        return StateTransactionResult::failure("internal/update_failed");
    }

    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        LOGW("%s: failed to sync cached alarm state", operation);
        return StateTransactionResult::failure("internal/update_failed");
    }
    *previousState = _state;
    const StateUpdateResult result = stateUpdater(_state);
    if (result == StateUpdateResult::ERROR) {
        LOGW("%s: updater returned ERROR", operation);
        _state = *previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
        LOGW("%s: failed to commit updated alarm state", operation);
        _state = *previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }

    return SYSTEM::STATE::finalizeStateTransaction(
        result,
        originId,
        propagate,
        [this](std::string_view handlerOriginId) {
            return callUpdateHandlers(handlerOriginId);
        },
        [this, previousState = std::move(previousState), operation]() {
            LOGW("%s: handler propagation failed, rolling back alarm settings", operation);
            const bool restored = rollbackToState(*previousState, true, "runStateUpdate/rollback");
            if (!restored) {
                LOGE("%s: rollback failed after handler failure", operation);
            }
            return restored;
        });
}

StateTransactionResult AlarmSettingsService::runJsonStateUpdate(
    JsonObject& jsonObject,
    const JsonStateUpdater<AlarmRulesSnapshot>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<AlarmRulesSnapshot>();
    if (!previousState) {
        LOGE("%s: failed to allocate previous alarm state in PSRAM", operation);
        return StateTransactionResult::failure("internal/update_failed");
    }

    SYSTEM::RecursiveScopeLock lock(_accessMutex, kAlarmSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        LOGW("%s: failed to sync cached alarm state", operation);
        return StateTransactionResult::failure("internal/update_failed");
    }
    *previousState = _state;
    const StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
    if (result == StateUpdateResult::ERROR) {
        LOGW("%s: JSON updater returned ERROR", operation);
        _state = *previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
        LOGW("%s: failed to commit updated alarm state", operation);
        _state = *previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }

    return SYSTEM::STATE::finalizeStateTransaction(
        result,
        originId,
        propagate,
        [this](std::string_view handlerOriginId) {
            return callUpdateHandlers(handlerOriginId);
        },
        [this, previousState = std::move(previousState), operation]() {
            LOGW("%s: handler propagation failed, rolling back alarm settings", operation);
            const bool restored = rollbackToState(*previousState, true, "runJsonStateUpdate/rollback");
            if (!restored) {
                LOGE("%s: rollback failed after handler failure", operation);
            }
            return restored;
        });
}

}  // namespace ALARMS
