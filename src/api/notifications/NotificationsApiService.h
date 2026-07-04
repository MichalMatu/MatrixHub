#pragma once

/**
 * @file NotificationsApiService.h
 * @brief Routing facade for notification API endpoints
 *
 * Responsibilities:
 * - Register HTTP endpoints with authentication
 * - Delegate requests to specialized handlers
 * - Initialize test modules (TelegramTest, WebhookTest, PushoverTest)
 *
 * Refactored: Jan 2026 (301 LOC → ~60 LOC)
 */

#include <PsychicHttpServer.h>
#include <security/SecurityManager.h>
#include "../BaseApiService.h"
#include "handlers/NotificationTestHandler.h"

namespace API {

class TelegramTestSender;
class WebhookTestSender;
class PushoverTestSender;

/**
 * @brief Routing facade for notification test endpoints
 *
 * Registers POST /api/notifications/{telegram,webhook,pushover}/test routes
 * and delegates to internal handler methods.
 */
class NotificationsApiService : public BaseApiService {
public:
    NotificationsApiService(
        PsychicHttpServer* server, 
        SecurityManager* securityManager, 
        POWER::PowerManager* powerManager,
        TelegramTestSender* telegramTestSender,
        WebhookTestSender* webhookTestSender,
        PushoverTestSender* pushoverTestSender
    );

    /**
     * @brief Register notification endpoints with authentication
     */
    void begin() override;

    // Explicit shutdown hook for detached API test jobs. The service still
    // drains them in destructors later, but restart/deep-sleep paths call this
    // early so background test traffic unwinds before WiFi is disabled.
    void shutdown();

private:
    Handlers::NotificationTestHandler _testHandler;
};

}  // namespace API
