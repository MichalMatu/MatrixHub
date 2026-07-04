/**
 * @file HeartbeatSettingsService.cpp
 * @brief Transactional settings service for heartbeat configuration
 */

#include "HeartbeatSettingsService.h"

#include "../../../core/config/ConfigManager.h"
#include "../../logging/Logging.h"
#include "../../memory/SystemAllocator.h"
#include "../../state/TransactionalStateHelpers.h"
#include "../../utils/ScopeLock.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "HeartbeatSettings"

namespace {
constexpr TickType_t kHeartbeatSettingsMutexTimeout = pdMS_TO_TICKS(5000);
}

namespace SYSTEM {

HeartbeatSettingsService::HeartbeatSettingsService(FS* fs, std::function<void()> onConfigApplied)
    : _fs(fs),
      _onConfigApplied(std::move(onConfigApplied)),
      _accessMutex(xSemaphoreCreateRecursiveMutex()) {
    syncCachedStateLocked();

    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistAndApply();
        },
        false);
}

HeartbeatSettingsService::~HeartbeatSettingsService() {
    if (_accessMutex) {
        vSemaphoreDelete(_accessMutex);
    }
}

void HeartbeatSettingsService::readState(RTC::HeartbeatData& settings, JsonObject& root) {
    root[CONFIG::Keys::kIntervalMs] = settings.intervalMs;
    JsonArray slots = root[CONFIG::Keys::kSlots].to<JsonArray>();
    for (uint8_t i = 0; i < RTC::kMaxHeartbeatSlots; i++) {
        const auto& slotState = settings.slots[i];
        JsonObject slot = slots.add<JsonObject>();
        slot[CONFIG::Keys::kEnabled] = slotState.enabled;
        slot[CONFIG::Keys::kName].set(String(slotState.name));
        slot[CONFIG::Keys::kUrl].set(String(slotState.url));
        slot[CONFIG::Keys::kAllowInsecure] = slotState.allowInsecure;
    }
}

StateUpdateResult HeartbeatSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::HeartbeatData& settings,
    std::string_view originId) {
    (void)originId;

    auto nextState = SYSTEM::MEMORY::makeUniqueInPsram<RTC::HeartbeatData>(settings);
    if (!nextState) {
        return StateUpdateResult::ERROR;
    }
    CONFIG::JSON::deserializeHeartbeat(jsonObject, *nextState);

    if (memcmp(&settings, nextState.get(), sizeof(RTC::HeartbeatData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = *nextState;
    return StateUpdateResult::CHANGED;
}

update_handler_id_t HeartbeatSettingsService::addUpdateHandler(StateUpdateCallback cb, bool allowRemove) {
    if (!cb) {
        return 0;
    }

    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, "addUpdateHandler")) {
        return 0;
    }

    static update_handler_id_t nextId = 1;
    const update_handler_id_t id = nextId++;
    _updateHandlers.push_back(UpdateHandler{id, cb, allowRemove});
    return id;
}

void HeartbeatSettingsService::removeUpdateHandler(update_handler_id_t id) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, "removeUpdateHandler")) {
        return;
    }

    _updateHandlers.remove_if([id](const UpdateHandler& handler) {
        return handler.allowRemove && handler.id == id;
    });
}

StateHandlerResult HeartbeatSettingsService::read(std::function<void(RTC::HeartbeatData&)> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, "read")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state);
    return StateHandlerResult::success();
}

StateHandlerResult HeartbeatSettingsService::read(JsonObject& jsonObject, JsonStateReader<RTC::HeartbeatData> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, "read(json)")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state, jsonObject);
    return StateHandlerResult::success();
}

StateTransactionResult HeartbeatSettingsService::updateAndPropagate(
    std::function<StateUpdateResult(RTC::HeartbeatData&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, true, "update");
}

StateTransactionResult HeartbeatSettingsService::updateAndPropagate(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::HeartbeatData> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, true, "update(json)");
}

StateUpdateResult HeartbeatSettingsService::update(
    std::function<StateUpdateResult(RTC::HeartbeatData&)> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(stateUpdater, originId).outcome;
}

StateUpdateResult HeartbeatSettingsService::update(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::HeartbeatData> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
}

StateUpdateResult HeartbeatSettingsService::updateWithoutPropagation(
    std::function<StateUpdateResult(RTC::HeartbeatData&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, false, "updateWithoutPropagation").outcome;
}

StateUpdateResult HeartbeatSettingsService::updateWithoutPropagation(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::HeartbeatData> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, false, "updateWithoutPropagation(json)").outcome;
}

StateHandlerResult HeartbeatSettingsService::callUpdateHandlers(std::string_view originId) {
    std::list<UpdateHandler> handlersCopy;
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, "callUpdateHandlers")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    handlersCopy = _updateHandlers;

    return SYSTEM::STATE::invokeUpdateHandlers(handlersCopy, originId);
}

bool HeartbeatSettingsService::ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const {
    if (lock.isLocked()) {
        return true;
    }

    LOGW("%s: mutex timeout", operation);
    return false;
}

bool HeartbeatSettingsService::syncCachedStateLocked() {
    bool synced = false;
    SYSTEM::HEARTBEAT_CONFIG::withConfig([&](const RTC::HeartbeatData& state) {
        _state = state;
        synced = true;
    });
    if (!synced) {
        LOGW("syncCachedStateLocked: heartbeat config unavailable");
    }
    return synced;
}

bool HeartbeatSettingsService::commitCachedStateLocked() {
    if (!SYSTEM::HEARTBEAT_CONFIG::update([this](RTC::HeartbeatData& state) {
            state = _state;
        })) {
        LOGW("commitCachedStateLocked: heartbeat config commit failed");
        return false;
    }

    return true;
}

bool HeartbeatSettingsService::rollbackToState(const RTC::HeartbeatData& state, const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return false;
    }

    _state = state;
    return commitCachedStateLocked();
}

StateHandlerResult HeartbeatSettingsService::persistAndApply() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist heartbeat settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    if (_onConfigApplied) {
        _onConfigApplied();
    }

    return StateHandlerResult::success();
}

StateTransactionResult HeartbeatSettingsService::runStateUpdate(
    const std::function<StateUpdateResult(RTC::HeartbeatData&)>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<RTC::HeartbeatData>();
    if (!previousState) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    *previousState = _state;
    const StateUpdateResult result = stateUpdater(_state);
    if (result == StateUpdateResult::ERROR) {
        _state = *previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
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
        [this, previousState = std::move(previousState)]() {
            return rollbackToState(*previousState, "runStateUpdate/rollback");
        });
}

StateTransactionResult HeartbeatSettingsService::runJsonStateUpdate(
    JsonObject& jsonObject,
    const JsonStateUpdater<RTC::HeartbeatData>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<RTC::HeartbeatData>();
    if (!previousState) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kHeartbeatSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    *previousState = _state;
    const StateUpdateResult result = stateUpdater(jsonObject, _state, originId);
    if (result == StateUpdateResult::ERROR) {
        _state = *previousState;
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (result == StateUpdateResult::CHANGED && !commitCachedStateLocked()) {
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
        [this, previousState = std::move(previousState)]() {
            return rollbackToState(*previousState, "runJsonStateUpdate/rollback");
        });
}

}  // namespace SYSTEM
