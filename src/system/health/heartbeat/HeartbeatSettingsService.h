/**
 * @file HeartbeatSettingsService.h
 * @brief Transactional settings service for heartbeat configuration
 */

#pragma once

#include <FS.h>
#include <list>

#include <core/StatefulService.h>

#include "../../../config/App.h"
#include "../../../config/json/SystemConfigJson.h"
#include "HeartbeatConfigStore.h"

namespace SYSTEM {

class HeartbeatSettingsService : public IStatefulService<RTC::HeartbeatData> {
public:
    explicit HeartbeatSettingsService(FS* fs, std::function<void()> onConfigApplied = {});
    ~HeartbeatSettingsService() override;

    static void readState(RTC::HeartbeatData& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::HeartbeatData& settings,
        std::string_view originId);

    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override;
    void removeUpdateHandler(update_handler_id_t id) override;

    StateHandlerResult read(std::function<void(RTC::HeartbeatData&)> stateReader) override;
    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<RTC::HeartbeatData> stateReader) override;

    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(RTC::HeartbeatData&)> stateUpdater,
                                              std::string_view originId) override;
    StateTransactionResult updateAndPropagate(JsonObject& jsonObject,
                                              JsonStateUpdater<RTC::HeartbeatData> stateUpdater,
                                              std::string_view originId) override;
    StateUpdateResult update(std::function<StateUpdateResult(RTC::HeartbeatData&)> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult update(JsonObject& jsonObject,
                             JsonStateUpdater<RTC::HeartbeatData> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(RTC::HeartbeatData&)> stateUpdater,
                                               std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(JsonObject& jsonObject,
                                               JsonStateUpdater<RTC::HeartbeatData> stateUpdater,
                                               std::string_view originId) override;

    StateHandlerResult callUpdateHandlers(std::string_view originId) override;

private:
    struct UpdateHandler {
        update_handler_id_t id;
        StateUpdateCallback callback;
        bool allowRemove;
    };

    bool ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const;
    bool syncCachedStateLocked();
    bool commitCachedStateLocked();
    bool rollbackToState(const RTC::HeartbeatData& state, const char* operation);
    StateHandlerResult persistAndApply();
    StateTransactionResult runStateUpdate(
        const std::function<StateUpdateResult(RTC::HeartbeatData&)>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);
    StateTransactionResult runJsonStateUpdate(
        JsonObject& jsonObject,
        const JsonStateUpdater<RTC::HeartbeatData>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);

    FS* _fs;
    std::function<void()> _onConfigApplied;
    SemaphoreHandle_t _accessMutex = nullptr;
    std::list<UpdateHandler> _updateHandlers;
    RTC::HeartbeatData _state{};
};

}  // namespace SYSTEM
