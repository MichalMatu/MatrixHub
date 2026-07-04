/**
 * @file NotificationSettingsService.cpp
 * @brief Unified notification settings service
 *
 * Refactored 3 Jan 2026: Uses centralized ConfigManager for persistence.
 */

#include "NotificationSettingsService.h"

#include "../../system/memory/SystemAllocator.h"
#include "../../system/logging/Logging.h"
#include "../../system/state/TransactionalStateHelpers.h"
#include "../../system/utils/ScopeLock.h"

#undef LOG_TAG
#define LOG_TAG "NotifySettings"

namespace {
constexpr TickType_t kServiceMutexTimeout = pdMS_TO_TICKS(5000);
}

NotificationSettingsService::NotificationSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager)
    : _httpEndpoint(NotificationSettingsAdapter::read,
                    NotificationSettingsAdapter::update,
                    this,
                    server,
                    API::PATH_NOTIFICATION_SETTINGS,
                    securityManager,
                    AuthenticationPredicates::IS_ADMIN),
      _fs(fs),
      _accessMutex(xSemaphoreCreateRecursiveMutex()) {
    syncCachedStateLocked();

    addUpdateHandler([&](std::string_view originId) {
        (void)originId;
        return onConfigUpdated();
    }, false);
}

NotificationSettingsService::~NotificationSettingsService() {
    if (_accessMutex) {
        vSemaphoreDelete(_accessMutex);
    }
}

void NotificationSettingsService::begin() {
    _httpEndpoint.begin();
    LOGI("Notification settings: Telegram=%d, Webhook=%d, Configured=%d",
         _state.telegramEnabled, _state.webhookEnabled, isConfigured());
}

bool NotificationSettingsService::snapshot(RTC::NotificationData& outState) const {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, "snapshot")) {
        outState = RTC::NotificationData{};
        return false;
    }

    outState = _state;
    return true;
}

bool NotificationSettingsService::isConfigured() const {
    RTC::NotificationData state{};
    return snapshot(state) && state.isConfigured();
}

bool NotificationSettingsService::hasChatId() const {
    RTC::NotificationData state{};
    return snapshot(state) && state.hasChatId();
}

void NotificationSettingsService::setChatId(const char* chatId) {
    if (!chatId || chatId[0] == '\0') return;

    const StateTransactionResult txResult = updateAndPropagate(
        [&](RTC::NotificationData& state) {
            if (state.hasChatId()) return StateUpdateResult::UNCHANGED;
            strlcpy(state.chatId, chatId, sizeof(state.chatId));
            return StateUpdateResult::CHANGED;
        },
        "telegram/autodiscovery");
    if (txResult.outcome == StateUpdateResult::ERROR) {
        LOGE("Failed to persist auto-discovered chat ID");
        return;
    }
    if (txResult.outcome == StateUpdateResult::CHANGED) {
        LOGI("Auto-discovered chat ID: %s", chatId);
    }
}

update_handler_id_t NotificationSettingsService::addUpdateHandler(StateUpdateCallback cb, bool allowRemove) {
    if (!cb) return 0;

    update_handler_id_t id = 0;
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!lock.isLocked()) {
        LOGW("addUpdateHandler: mutex timeout");
        return 0;
    }

    static update_handler_id_t nextId = 1;
    id = nextId++;
    _updateHandlers.push_back(UpdateHandler{id, cb, allowRemove});
    return id;
}

void NotificationSettingsService::removeUpdateHandler(update_handler_id_t id) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, "removeUpdateHandler")) {
        return;
    }

    _updateHandlers.remove_if([id](const UpdateHandler& handler) {
        return handler.allowRemove && handler.id == id;
    });
}

StateHandlerResult NotificationSettingsService::read(std::function<void(RTC::NotificationData&)> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, "read")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    return readStateLocked(stateReader)
        ? StateHandlerResult::success()
        : StateHandlerResult::failure("internal/update_failed");
}

StateHandlerResult NotificationSettingsService::read(JsonObject& jsonObject, JsonStateReader<RTC::NotificationData> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, "read(json)")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    return readStateLocked(jsonObject, stateReader)
        ? StateHandlerResult::success()
        : StateHandlerResult::failure("internal/update_failed");
}

StateTransactionResult NotificationSettingsService::updateAndPropagate(
    std::function<StateUpdateResult(RTC::NotificationData&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, true, "update");
}

StateTransactionResult NotificationSettingsService::updateAndPropagate(
    JsonObject& jsonObject,
    JsonStateUpdater<RTC::NotificationData> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, true, "update(json)");
}

StateUpdateResult NotificationSettingsService::update(std::function<StateUpdateResult(RTC::NotificationData&)> stateUpdater, std::string_view originId) {
    return updateAndPropagate(stateUpdater, originId).outcome;
}

StateUpdateResult NotificationSettingsService::update(JsonObject& jsonObject,
                                                      JsonStateUpdater<RTC::NotificationData> stateUpdater,
                                                      std::string_view originId) {
    return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
}

StateUpdateResult NotificationSettingsService::updateWithoutPropagation(std::function<StateUpdateResult(RTC::NotificationData&)> stateUpdater,
                                                                        std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, false, "updateWithoutPropagation").outcome;
}

StateUpdateResult NotificationSettingsService::updateWithoutPropagation(JsonObject& jsonObject,
                                                                        JsonStateUpdater<RTC::NotificationData> stateUpdater,
                                                                        std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, false, "updateWithoutPropagation(json)").outcome;
}

StateHandlerResult NotificationSettingsService::callUpdateHandlers(std::string_view originId) {
    std::list<UpdateHandler> handlersCopy;
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!lock.isLocked()) {
        LOGW("callUpdateHandlers: mutex timeout");
        return StateHandlerResult::failure("internal/update_failed");
    }
    handlersCopy = _updateHandlers;

    return SYSTEM::STATE::invokeUpdateHandlers(handlersCopy, originId);
}

StateHandlerResult NotificationSettingsService::onConfigUpdated() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist notification settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    LOGI("Notification settings saved to filesystem - applied without restart");
    return StateHandlerResult::success();
}

bool NotificationSettingsService::ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const {
    if (lock.isLocked()) {
        return true;
    }

    LOGW("%s: mutex timeout", operation);
    return false;
}

bool NotificationSettingsService::prepareStateLocked() {
    return syncCachedStateLocked();
}

bool NotificationSettingsService::rollbackToState(const RTC::NotificationData& state, const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return false;
    }

    _state = state;
    if (!NOTIFICATIONS::CONFIG_STORE::update([&](RTC::NotificationData& current) {
            current = state;
        })) {
        return false;
    }

    return true;
}

StateTransactionResult NotificationSettingsService::runStateUpdate(
    const std::function<StateUpdateResult(RTC::NotificationData&)>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<RTC::NotificationData>();
    if (!previousState) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!prepareStateLocked()) {
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

StateTransactionResult NotificationSettingsService::runJsonStateUpdate(
    JsonObject& jsonObject,
    const JsonStateUpdater<RTC::NotificationData>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<RTC::NotificationData>();
    if (!previousState) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kServiceMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!prepareStateLocked()) {
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
bool NotificationSettingsService::readStateLocked(const std::function<void(RTC::NotificationData&)>& stateReader) {
    if (!prepareStateLocked()) {
        return false;
    }

    stateReader(_state);
    return true;
}

bool NotificationSettingsService::readStateLocked(JsonObject& jsonObject,
                                                  const JsonStateReader<RTC::NotificationData>& stateReader) {
    if (!prepareStateLocked()) {
        return false;
    }

    stateReader(_state, jsonObject);
    return true;
}

bool NotificationSettingsService::syncCachedStateLocked() {
    bool synced = false;
    NOTIFICATIONS::CONFIG_STORE::withConfig([&](const RTC::NotificationData& state) {
        _state = state;
        synced = true;
    });
    if (!synced) {
        LOGW("syncCachedStateLocked: notification config unavailable");
    }
    return synced;
}

bool NotificationSettingsService::commitCachedStateLocked() {
    if (!NOTIFICATIONS::CONFIG_STORE::update([this](RTC::NotificationData& state) {
            state = _state;
        })) {
        LOGW("commitCachedStateLocked: notification config commit failed");
        return false;
    }
    return true;
}
