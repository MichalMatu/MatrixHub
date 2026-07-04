/**
 * @file PowerSettingsService.h
 * @brief Transactional settings service for power configuration
 */

#pragma once

#include <list>

#include <core/StatefulService.h>

#include "../../config/App.h"
#include "../../config/json/PowerSettingsJson.h"
#include "../rtc/RtcConfig.h"

namespace POWER {

class PowerSettingsService : public IStatefulService<RTC::PowerData> {
public:
    explicit PowerSettingsService(std::function<bool(const RTC::PowerData&)> applyConfig = {});
    ~PowerSettingsService() override;

    static void readState(RTC::PowerData& settings, JsonObject& root);
    static StateHandlerResult validateStateUpdate(JsonObject& jsonObject);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        RTC::PowerData& settings,
        std::string_view originId);

    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override;
    void removeUpdateHandler(update_handler_id_t id) override;

    StateHandlerResult read(std::function<void(RTC::PowerData&)> stateReader) override;
    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<RTC::PowerData> stateReader) override;

    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(RTC::PowerData&)> stateUpdater,
                                              std::string_view originId) override;
    StateTransactionResult updateAndPropagate(JsonObject& jsonObject,
                                              JsonStateUpdater<RTC::PowerData> stateUpdater,
                                              std::string_view originId) override;
    StateUpdateResult update(std::function<StateUpdateResult(RTC::PowerData&)> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult update(JsonObject& jsonObject,
                             JsonStateUpdater<RTC::PowerData> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(RTC::PowerData&)> stateUpdater,
                                               std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(JsonObject& jsonObject,
                                               JsonStateUpdater<RTC::PowerData> stateUpdater,
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
    bool applyStateLocked(const RTC::PowerData& state);
    bool rollbackToState(const RTC::PowerData& state, bool propagate, const char* operation);
    StateTransactionResult runStateUpdate(
        const std::function<StateUpdateResult(RTC::PowerData&)>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);
    StateTransactionResult runJsonStateUpdate(
        JsonObject& jsonObject,
        const JsonStateUpdater<RTC::PowerData>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);

    std::function<bool(const RTC::PowerData&)> _applyConfig;
    SemaphoreHandle_t _accessMutex = nullptr;
    std::list<UpdateHandler> _updateHandlers;
    RTC::PowerData _state{};
};

}  // namespace POWER
