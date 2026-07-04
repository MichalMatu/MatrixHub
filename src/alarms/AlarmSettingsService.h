/**
 * @file AlarmSettingsService.h
 * @brief Transactional settings service for alarm rules
 */

#pragma once

#include <FS.h>
#include <list>

#include <core/StatefulService.h>

#include "../system/rtc/types/RtcAlarmTypes.h"

namespace ALARMS {

class AlarmSettingsService : public IStatefulService<AlarmRulesSnapshot> {
public:
    explicit AlarmSettingsService(
        FS* fs,
        std::function<bool(const AlarmRulesSnapshot&)> applyRules = {});
    ~AlarmSettingsService() override;

    static void readState(AlarmRulesSnapshot& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        AlarmRulesSnapshot& settings,
        std::string_view originId);
    static StateHandlerResult validateStateUpdate(JsonObject& jsonObject);

    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override;
    void removeUpdateHandler(update_handler_id_t id) override;

    StateHandlerResult read(std::function<void(AlarmRulesSnapshot&)> stateReader) override;
    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<AlarmRulesSnapshot> stateReader) override;

    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(AlarmRulesSnapshot&)> stateUpdater,
                                              std::string_view originId) override;
    StateTransactionResult updateAndPropagate(JsonObject& jsonObject,
                                              JsonStateUpdater<AlarmRulesSnapshot> stateUpdater,
                                              std::string_view originId) override;
    StateUpdateResult update(std::function<StateUpdateResult(AlarmRulesSnapshot&)> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult update(JsonObject& jsonObject,
                             JsonStateUpdater<AlarmRulesSnapshot> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(AlarmRulesSnapshot&)> stateUpdater,
                                               std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(JsonObject& jsonObject,
                                               JsonStateUpdater<AlarmRulesSnapshot> stateUpdater,
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
    bool persistStateSnapshot(const AlarmRulesSnapshot& state) const;
    bool applyStateSnapshot(const AlarmRulesSnapshot& state) const;
    bool rollbackToState(const AlarmRulesSnapshot& state, bool propagate, const char* operation);
    StateHandlerResult persistAndApply();
    StateTransactionResult runStateUpdate(
        const std::function<StateUpdateResult(AlarmRulesSnapshot&)>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);
    StateTransactionResult runJsonStateUpdate(
        JsonObject& jsonObject,
        const JsonStateUpdater<AlarmRulesSnapshot>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);

    FS* _fs;
    std::function<bool(const AlarmRulesSnapshot&)> _applyRules;
    SemaphoreHandle_t _accessMutex = nullptr;
    std::list<UpdateHandler> _updateHandlers;
    AlarmRulesSnapshot _state{};
};

}  // namespace ALARMS
