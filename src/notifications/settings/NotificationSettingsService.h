#pragma once

#include <list>

#include <core/StatefulService.h>

#include "../../config/App.h"
#include "core/config/ConfigManager.h"
#include "NotificationSettingsAdapter.h"
#include "NotificationConfigStore.h"
#include <core/HttpEndpoint.h>

/**
 * @brief Notification Settings Service
 * 
 * Manages unified notification configuration in a PSRAM-backed config store.
 * RTC keeps only a compact retained summary for boot diagnostics.
 */
class NotificationSettingsService : public IStatefulService<RTC::NotificationData> {
public:
    NotificationSettingsService(PsychicHttpServer* server, FS* fs, SecurityManager* securityManager);
    ~NotificationSettingsService() override;

    void begin();

    /**
     * @brief Thread-safe snapshot of the current notification settings.
     * @return true when snapshot was captured, false on mutex timeout.
     */
    bool snapshot(RTC::NotificationData& outState) const;

    // Overall configured check (any service ready)
    bool isConfigured() const;

    bool hasChatId() const;

    // Auto-discovery: set chat ID from first incoming message
    void setChatId(const char* chatId);

    update_handler_id_t addUpdateHandler(StateUpdateCallback cb, bool allowRemove = true) override;
    void removeUpdateHandler(update_handler_id_t id) override;

    StateHandlerResult read(std::function<void(RTC::NotificationData&)> stateReader) override;
    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<RTC::NotificationData> stateReader) override;

    StateTransactionResult updateAndPropagate(std::function<StateUpdateResult(RTC::NotificationData&)> stateUpdater, std::string_view originId) override;
    StateTransactionResult updateAndPropagate(JsonObject& jsonObject, JsonStateUpdater<RTC::NotificationData> stateUpdater, std::string_view originId) override;
    StateUpdateResult update(std::function<StateUpdateResult(RTC::NotificationData&)> stateUpdater, std::string_view originId) override;
    StateUpdateResult update(JsonObject& jsonObject, JsonStateUpdater<RTC::NotificationData> stateUpdater, std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(std::function<StateUpdateResult(RTC::NotificationData&)> stateUpdater, std::string_view originId) override;
    StateUpdateResult updateWithoutPropagation(JsonObject& jsonObject, JsonStateUpdater<RTC::NotificationData> stateUpdater, std::string_view originId) override;

    StateHandlerResult callUpdateHandlers(std::string_view originId) override;

private:
    struct UpdateHandler {
        update_handler_id_t id;
        StateUpdateCallback callback;
        bool allowRemove;
    };

    HttpEndpoint<RTC::NotificationData> _httpEndpoint;
    FS* _fs;
    SemaphoreHandle_t _accessMutex = nullptr;
    std::list<UpdateHandler> _updateHandlers;
    RTC::NotificationData _state{};

    StateHandlerResult onConfigUpdated();
    bool ensureLocked(const SYSTEM::RecursiveScopeLock& lock, const char* operation) const;
    bool prepareStateLocked();
    bool rollbackToState(const RTC::NotificationData& state, const char* operation);
    StateTransactionResult runStateUpdate(const std::function<StateUpdateResult(RTC::NotificationData&)>& stateUpdater,
                                          std::string_view originId,
                                          bool propagate,
                                          const char* operation);
    StateTransactionResult runJsonStateUpdate(JsonObject& jsonObject,
                                              const JsonStateUpdater<RTC::NotificationData>& stateUpdater,
                                              std::string_view originId,
                                              bool propagate,
                                              const char* operation);
    bool readStateLocked(const std::function<void(RTC::NotificationData&)>& stateReader);
    bool readStateLocked(JsonObject& jsonObject, const JsonStateReader<RTC::NotificationData>& stateReader);
    bool syncCachedStateLocked();
    bool commitCachedStateLocked();
};
