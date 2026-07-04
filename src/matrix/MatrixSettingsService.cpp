/**
 * @file MatrixSettingsService.cpp
 * @brief Transactional Matrix LED settings service
 */

#include "MatrixSettingsService.h"

#include <algorithm>
#include <cstring>

#include "../config/json/MatrixConfigJson.h"
#include "../alarms/AlarmService.h"
#include "../matrix/MatrixDataVisualizationTypes.h"
#include "../matrix/MatrixEffectModes.h"
#include "../system/logging/Logging.h"
#include "../system/memory/SystemAllocator.h"
#include "../system/state/TransactionalStateHelpers.h"

#undef LOG_TAG
#define LOG_TAG "MatrixSettings"

namespace {
constexpr TickType_t kMatrixSettingsMutexTimeout = pdMS_TO_TICKS(5000);

}  // namespace

namespace MATRIX {

MatrixSettingsService::MatrixSettingsService(
    fs::FS* fs,
    MatrixService* matrixService,
    ALARMS::AlarmService* alarmService,
    MATRIX_MANAGER::MatrixManagerService* matrixManager,
    MatrixMenuService* menuService)
    : _accessMutex(xSemaphoreCreateRecursiveMutex()),
      _fs(fs),
      _matrixService(matrixService),
      _alarmService(alarmService),
      _matrixManager(matrixManager),
      _menuService(menuService),
      _store(fs),
      _runtimeApplier(matrixService, alarmService, matrixManager, menuService) {
    configASSERT(_fs != nullptr);
    configASSERT(_matrixService != nullptr);

    (void)syncCachedStateLocked();
}

MatrixSettingsService::~MatrixSettingsService() {
    if (_accessMutex) {
        vSemaphoreDelete(_accessMutex);
    }
}

void MatrixSettingsService::begin() {
    RTC::updateConfig([](RTC::ConfigStore& store) {
        auto& menu = store.matrix.menu;
        if (menu.textColor == 0) {
            menu.textColor = UI::MATRIX::MENU_TEXT_COLOR_DEFAULT;
        }
        if (menu.scrollSpeed == 0) {
            menu.scrollSpeed = UI::MATRIX::SCROLL_INTERVAL_MS;
        }
    });

    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    if (snapshot && snapshotState(*snapshot)) {
        applyRuntimeState(*snapshot);
    }

    LOGI("Settings (RTC): brightness=%u, mode=%u, rot=%u, autoRot=%d",
         getBrightness(),
         static_cast<uint8_t>(getAlarmMode()),
         getRotation(),
         isAutoRotate());
}

void MatrixSettingsService::readState(MatrixSettingsState& settings, JsonObject& root) {
    CONFIG::JSON::serializeMatrix(settings.config, root);
    MatrixCustomIconsCodec::serialize(settings.customIcons, root);
}

StateUpdateResult MatrixSettingsService::updateState(
    JsonObject& jsonObject,
    MatrixSettingsState& settings,
    std::string_view originId) {
    (void)originId;

    auto nextState = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>(settings);
    if (!nextState) {
        return StateUpdateResult::ERROR;
    }
    CONFIG::JSON::deserializeMatrix(jsonObject, nextState->config);
    MatrixCustomIconsCodec::deserialize(jsonObject, nextState->customIcons);

    const bool configChanged =
        memcmp(&settings.config, &nextState->config, sizeof(RTC::MatrixData)) != 0;
    const bool iconsChanged =
        !MatrixCustomIconsCodec::equal(settings.customIcons, nextState->customIcons);
    if (!configChanged && !iconsChanged) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = *nextState;
    return StateUpdateResult::CHANGED;
}

update_handler_id_t MatrixSettingsService::addUpdateHandler(StateUpdateCallback cb, bool allowRemove) {
    if (!cb) {
        return 0;
    }

    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
    if (!ensureLocked(lock, "addUpdateHandler")) {
        return 0;
    }

    static update_handler_id_t nextId = 1;
    const update_handler_id_t id = nextId++;
    _updateHandlers.push_back(UpdateHandler{id, cb, allowRemove});
    return id;
}

void MatrixSettingsService::removeUpdateHandler(update_handler_id_t id) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
    if (!ensureLocked(lock, "removeUpdateHandler")) {
        return;
    }

    _updateHandlers.remove_if([id](const UpdateHandler& handler) {
        return handler.allowRemove && handler.id == id;
    });
}

StateHandlerResult MatrixSettingsService::read(std::function<void(MatrixSettingsState&)> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
    if (!ensureLocked(lock, "read")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state);
    return StateHandlerResult::success();
}

StateHandlerResult MatrixSettingsService::read(JsonObject& jsonObject, JsonStateReader<MatrixSettingsState> stateReader) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
    if (!ensureLocked(lock, "read(json)")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    if (!syncCachedStateLocked()) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    stateReader(_state, jsonObject);
    return StateHandlerResult::success();
}

StateTransactionResult MatrixSettingsService::updateAndPropagate(
    std::function<StateUpdateResult(MatrixSettingsState&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, true, "update");
}

StateTransactionResult MatrixSettingsService::updateAndPropagate(
    JsonObject& jsonObject,
    JsonStateUpdater<MatrixSettingsState> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, true, "update(json)");
}

StateUpdateResult MatrixSettingsService::update(
    std::function<StateUpdateResult(MatrixSettingsState&)> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(stateUpdater, originId).outcome;
}

StateUpdateResult MatrixSettingsService::update(
    JsonObject& jsonObject,
    JsonStateUpdater<MatrixSettingsState> stateUpdater,
    std::string_view originId) {
    return updateAndPropagate(jsonObject, stateUpdater, originId).outcome;
}

StateUpdateResult MatrixSettingsService::updateWithoutPropagation(
    std::function<StateUpdateResult(MatrixSettingsState&)> stateUpdater,
    std::string_view originId) {
    return runStateUpdate(stateUpdater, originId, false, "updateWithoutPropagation").outcome;
}

StateUpdateResult MatrixSettingsService::updateWithoutPropagation(
    JsonObject& jsonObject,
    JsonStateUpdater<MatrixSettingsState> stateUpdater,
    std::string_view originId) {
    return runJsonStateUpdate(jsonObject, stateUpdater, originId, false, "updateWithoutPropagation(json)").outcome;
}

StateHandlerResult MatrixSettingsService::callUpdateHandlers(std::string_view originId) {
    std::list<UpdateHandler> handlersCopy;
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
    if (!ensureLocked(lock, "callUpdateHandlers")) {
        return StateHandlerResult::failure("internal/update_failed");
    }
    handlersCopy = _updateHandlers;

    return SYSTEM::STATE::invokeUpdateHandlers(handlersCopy, originId);
}

uint8_t MatrixSettingsService::getBrightness() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.brightness : 0;
}

RTC::MatrixAlarmMode MatrixSettingsService::getAlarmMode() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot)
        ? snapshot->config.alarmMode
        : RTC::MatrixAlarmMode::SCROLL_TEXT;
}

uint8_t MatrixSettingsService::getRotation() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.rotation : 0;
}

bool MatrixSettingsService::isAutoRotate() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.autoRotate : false;
}

bool MatrixSettingsService::getEffectEnabled() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.effectEnabled : false;
}

uint8_t MatrixSettingsService::getEffectMode() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.effectMode : 0;
}

uint32_t MatrixSettingsService::getEffectSpeed() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.effectSpeed : 0;
}

uint32_t MatrixSettingsService::getEffectColor() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.effectColor : 0;
}

uint32_t MatrixSettingsService::getEffectColor2() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.effectColor2 : 0;
}

uint32_t MatrixSettingsService::getEffectColor3() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.effectColor3 : 0;
}

uint32_t MatrixSettingsService::getMenuTextColor() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.menu.textColor : 0;
}

uint16_t MatrixSettingsService::getMenuScrollSpeed() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.menu.scrollSpeed : 0;
}

bool MatrixSettingsService::isMenuEnabled() const {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    return snapshot && snapshotState(*snapshot) ? snapshot->config.menu.enabled : false;
}

bool MatrixSettingsService::forceUpdate() {
    return persistAndApplyCurrentState().ok;
}

bool MatrixSettingsService::ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const {
    if (lock.isLocked()) {
        return true;
    }

    LOGW("%s: mutex timeout", operation);
    return false;
}

bool MatrixSettingsService::snapshotState(MatrixSettingsState& outState) const {
    auto* self = const_cast<MatrixSettingsService*>(this);
    SYSTEM::RecursiveScopeLock lock(self->_accessMutex, kMatrixSettingsMutexTimeout);
    if (!self->ensureLocked(lock, "snapshotState")) {
        outState = MatrixSettingsState{};
        return false;
    }
    if (!self->syncCachedStateLocked()) {
        outState = MatrixSettingsState{};
        return false;
    }
    outState = self->_state;
    return true;
}

bool MatrixSettingsService::syncCachedStateLocked() {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    if (!snapshot) {
        return false;
    }
    if (!_store.sync(*snapshot)) {
        LOGW("syncCachedStateLocked: matrix RTC config unavailable");
        return false;
    }

    // Re-sanitize cached RTC data on read so older persisted configs are healed
    // even if they were saved before effect-mode validation existed.
    snapshot->config.effectEngine = normalizeMatrixEffectEngine(snapshot->config.effectEngine);
    snapshot->config.effectMode = normalizeMatrixEffectModeForEngine(
        snapshot->config.effectMode,
        snapshot->config.effectEngine);
    snapshot->config.effectReactivityProvider = normalizeMatrixEffectReactivityProvider(
        snapshot->config.effectReactivityProvider);
    snapshot->config.effectReactivityGain = normalizeMatrixEffectReactivityGain(
        snapshot->config.effectReactivityGain);
    snapshot->config.backgroundMode = normalizeMatrixBackgroundMode(snapshot->config.backgroundMode);
    snapshot->config.dataVisualizationSource =
        normalizeMatrixDataSource(snapshot->config.dataVisualizationSource);
    snapshot->config.dataVisualizationMetric =
        normalizeMatrixDataMetric(snapshot->config.dataVisualizationMetric);
    snapshot->config.dataVisualizationMode =
        normalizeMatrixDataVizMode(snapshot->config.dataVisualizationMode);
    snapshot->config.dataVisualizationStaleBehavior =
        normalizeMatrixDataStaleBehavior(snapshot->config.dataVisualizationStaleBehavior);
    snapshot->config.dataVisualizationColorMin =
        normalizeMatrixDataColor(snapshot->config.dataVisualizationColorMin);
    snapshot->config.dataVisualizationColorMid =
        normalizeMatrixDataColor(snapshot->config.dataVisualizationColorMid);
    snapshot->config.dataVisualizationColorMax =
        normalizeMatrixDataColor(snapshot->config.dataVisualizationColorMax);
    if (snapshot->config.dataVisualizationMax <= snapshot->config.dataVisualizationMin) {
        snapshot->config.dataVisualizationMax = snapshot->config.dataVisualizationMin + 1.0f;
    }
    if (snapshot->config.dataVisualizationBrightnessMax < snapshot->config.dataVisualizationBrightnessMin) {
        std::swap(snapshot->config.dataVisualizationBrightnessMax,
                  snapshot->config.dataVisualizationBrightnessMin);
    }
    normalizeMatrixBackgroundSelection(snapshot->config);
    _state = *snapshot;
    return true;
}

bool MatrixSettingsService::writeStateToStoresLocked(const MatrixSettingsState& state) {
    if (!_store.write(state)) {
        LOGW("writeStateToStoresLocked: matrix RTC commit failed");
        return false;
    }

    _state = state;
    return true;
}

bool MatrixSettingsService::commitCachedStateLocked() {
    return writeStateToStoresLocked(_state);
}

bool MatrixSettingsService::persistState(const MatrixSettingsState& state) const {
    if (!_store.persist(state)) {
        LOGE("Failed to persist matrix settings");
        return false;
    }

    return true;
}

void MatrixSettingsService::applyRuntimeState(const MatrixSettingsState& state) const {
    _runtimeApplier.apply(state);
    LOGD("Applied brightness: %u", state.config.brightness);
}

StateHandlerResult MatrixSettingsService::persistAndApplyCurrentState() {
    auto snapshot = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    if (!snapshot || !snapshotState(*snapshot)) {
        return StateHandlerResult::failure("internal/update_failed");
    }

    if (!persistState(*snapshot)) {
        return StateHandlerResult::failure("config/save_failed");
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    applyRuntimeState(*snapshot);
    return StateHandlerResult::success();
}

bool MatrixSettingsService::rollbackToState(
    const MatrixSettingsState& state,
    bool persistAndApply,
    const char* operation) {
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
    if (!ensureLocked(lock, operation)) {
        return false;
    }

    if (!writeStateToStoresLocked(state)) {
        return false;
    }
    lock.unlock();

    if (!persistAndApply) {
        return true;
    }

    if (!persistState(state)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(50));
    applyRuntimeState(state);
    return true;
}

StateTransactionResult MatrixSettingsService::runStateUpdate(
    const std::function<StateUpdateResult(MatrixSettingsState&)>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    if (!previousState) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
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
        if (!rollbackToState(*previousState, false, "runStateUpdate/rollback")) {
            return StateTransactionResult::failure("rtc/restore_failed");
        }
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!propagate || result != StateUpdateResult::CHANGED) {
        return StateTransactionResult::fromOutcome(result);
    }

    const StateHandlerResult persistResult = persistAndApplyCurrentState();
    if (!persistResult.ok) {
        if (!rollbackToState(*previousState, false, "runStateUpdate/persistRollback")) {
            return StateTransactionResult::failure("rtc/restore_failed");
        }
        return StateTransactionResult::failure(
            persistResult.errorCode ? persistResult.errorCode : "internal/update_failed",
            persistResult.httpStatus);
    }

    const StateHandlerResult handlerResult = callUpdateHandlers(originId);
    if (handlerResult.ok) {
        return StateTransactionResult::fromOutcome(result);
    }

    if (!rollbackToState(*previousState, true, "runStateUpdate/handlerRollback")) {
        return StateTransactionResult::failure("rtc/restore_failed");
    }

    return StateTransactionResult::failure(
        handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
        handlerResult.httpStatus);
}

StateTransactionResult MatrixSettingsService::runJsonStateUpdate(
    JsonObject& jsonObject,
    const JsonStateUpdater<MatrixSettingsState>& stateUpdater,
    std::string_view originId,
    bool propagate,
    const char* operation) {
    auto previousState = SYSTEM::MEMORY::makeUniqueInPsram<MatrixSettingsState>();
    if (!previousState) {
        return StateTransactionResult::failure("internal/update_failed");
    }
    SYSTEM::RecursiveScopeLock lock(_accessMutex, kMatrixSettingsMutexTimeout);
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
        if (!rollbackToState(*previousState, false, "runJsonStateUpdate/rollback")) {
            return StateTransactionResult::failure("rtc/restore_failed");
        }
        return StateTransactionResult::failure("internal/update_failed");
    }
    if (!propagate || result != StateUpdateResult::CHANGED) {
        return StateTransactionResult::fromOutcome(result);
    }

    const StateHandlerResult persistResult = persistAndApplyCurrentState();
    if (!persistResult.ok) {
        if (!rollbackToState(*previousState, false, "runJsonStateUpdate/persistRollback")) {
            return StateTransactionResult::failure("rtc/restore_failed");
        }
        return StateTransactionResult::failure(
            persistResult.errorCode ? persistResult.errorCode : "internal/update_failed",
            persistResult.httpStatus);
    }

    const StateHandlerResult handlerResult = callUpdateHandlers(originId);
    if (handlerResult.ok) {
        return StateTransactionResult::fromOutcome(result);
    }

    if (!rollbackToState(*previousState, true, "runJsonStateUpdate/handlerRollback")) {
        return StateTransactionResult::failure("rtc/restore_failed");
    }

    return StateTransactionResult::failure(
        handlerResult.errorCode ? handlerResult.errorCode : "internal/update_failed",
        handlerResult.httpStatus);
}

}  // namespace MATRIX
