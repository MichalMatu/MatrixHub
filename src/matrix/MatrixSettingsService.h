/**
 * @file MatrixSettingsService.h
 * @brief Transactional settings service for Matrix LED configuration
 */

#pragma once

#include <list>
#include <FS.h>

#include <core/StatefulService.h>

#include "../system/rtc/RtcConfig.h"
#include "../system/utils/ScopeLock.h"
#include "../config/System.h"
#include "MatrixRuntimeApplier.h"
#include "MatrixSettingsStore.h"
#include "MatrixSettingsTypes.h"

class MatrixService;
namespace ALARMS { class AlarmService; }
namespace MATRIX { class MatrixMenuService; }
namespace MATRIX_MANAGER { class MatrixManagerService; }

namespace MATRIX {
/**
 * @brief Transactional Matrix LED settings service.
 */
class MatrixSettingsService : public IStatefulService<MatrixSettingsState> {
public:
    MatrixSettingsService(fs::FS* fs, MatrixService* matrixService, ALARMS::AlarmService* alarmService, MATRIX_MANAGER::MatrixManagerService* matrixManager = nullptr, MatrixMenuService* menuService = nullptr);
    ~MatrixSettingsService() override;

    void begin();

    static void readState(MatrixSettingsState& settings, JsonObject& root);
    static StateUpdateResult updateState(
        JsonObject& jsonObject,
        MatrixSettingsState& settings,
        std::string_view originId);

    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override;
    void removeUpdateHandler(update_handler_id_t id) override;

    StateHandlerResult read(std::function<void(MatrixSettingsState&)> stateReader) override;
    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<MatrixSettingsState> stateReader) override;

    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(MatrixSettingsState&)> stateUpdater,
                                              std::string_view originId) override;
    StateTransactionResult updateAndPropagate(JsonObject& jsonObject,
                                              JsonStateUpdater<MatrixSettingsState> stateUpdater,
                                              std::string_view originId) override;
    StateUpdateResult update(std::function<StateUpdateResult(MatrixSettingsState&)> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult update(JsonObject& jsonObject,
                             JsonStateUpdater<MatrixSettingsState> stateUpdater,
                             std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(MatrixSettingsState&)> stateUpdater,
                                               std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(JsonObject& jsonObject,
                                               JsonStateUpdater<MatrixSettingsState> stateUpdater,
                                               std::string_view originId) override;

    StateHandlerResult callUpdateHandlers(std::string_view originId) override;

    // Accessors
    uint8_t getBrightness() const;
    RTC::MatrixAlarmMode getAlarmMode() const;

    uint8_t getRotation() const;
    bool isAutoRotate() const;
    
    // Effect Accessors
    bool getEffectEnabled() const;
    uint8_t getEffectMode() const;
    uint32_t getEffectSpeed() const;
    uint32_t getEffectColor() const;
    uint32_t getEffectColor2() const;
    uint32_t getEffectColor3() const;
    
    // Menu Accessors
    uint32_t getMenuTextColor() const;
    uint16_t getMenuScrollSpeed() const;
    bool isMenuEnabled() const;

    // Force persist + hardware reapply for the current state snapshot.
    bool forceUpdate();

private:
    struct UpdateHandler {
        update_handler_id_t id;
        StateUpdateCallback callback;
        bool allowRemove;
    };

    bool ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const;
    bool snapshotState(MatrixSettingsState& outState) const;
    bool syncCachedStateLocked();
    bool writeStateToStoresLocked(const MatrixSettingsState& state);
    bool commitCachedStateLocked();
    bool persistState(const MatrixSettingsState& state) const;
    void applyRuntimeState(const MatrixSettingsState& state) const;
    StateHandlerResult persistAndApplyCurrentState();
    bool rollbackToState(const MatrixSettingsState& state, bool persistAndApply, const char* operation);
    StateTransactionResult runStateUpdate(
        const std::function<StateUpdateResult(MatrixSettingsState&)>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);
    StateTransactionResult runJsonStateUpdate(
        JsonObject& jsonObject,
        const JsonStateUpdater<MatrixSettingsState>& stateUpdater,
        std::string_view originId,
        bool propagate,
        const char* operation);

    SemaphoreHandle_t _accessMutex = nullptr;
    std::list<UpdateHandler> _updateHandlers;
    MatrixSettingsState _state{};
    
    fs::FS* _fs;
    MatrixService* _matrixService;
    ALARMS::AlarmService* _alarmService;
    MATRIX_MANAGER::MatrixManagerService* _matrixManager;
    MatrixMenuService* _menuService;
    MatrixSettingsStore _store;
    MatrixRuntimeApplier _runtimeApplier;
};

}  // namespace MATRIX
