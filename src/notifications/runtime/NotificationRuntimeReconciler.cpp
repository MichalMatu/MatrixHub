#include "NotificationRuntimeReconciler.h"

#include "../settings/NotificationSettingsService.h"
#include "NotificationWorker.h"
#include "../../system/logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "NotifRuntime"

namespace NOTIFICATIONS {

NotificationRuntimeSnapshot NotificationRuntimeReconciler::snapshot(const NotificationSettingsService* settings,
                                                                   const NotificationWorker* worker) {
    NotificationRuntimeSnapshot state;
    if (worker) {
        state.workerRunning = worker->isRunning();
    }

    if (settings) {
        RTC::NotificationData cfg{};
        state.settingsSnapshotValid = settings->snapshot(cfg);
        if (state.settingsSnapshotValid) {
            state.telegramEnabled = cfg.telegramEnabled;
            state.webhookEnabled = cfg.webhookEnabled;
            state.pushoverEnabled = cfg.pushoverEnabled;
        }
    }
    return state;
}

void NotificationRuntimeReconciler::reconcile(const NotificationSettingsService* settings,
                                              NotificationWorker* worker) {
    if (!settings || !worker) {
        return;
    }

    const NotificationRuntimeSnapshot state = snapshot(settings, worker);
    switch (computeTransition(state)) {
        case NotificationWorkerTransition::Start:
            LOGI("Starting unified worker after notification settings update");
            worker->begin();
            break;
        case NotificationWorkerTransition::Stop:
            LOGI("Stopping unified worker because all notification channels are disabled");
            worker->stop();
            // Edge case hardening:
            // stop() can take a bounded amount of time while transports unwind.
            // If settings flip back to enabled during that window, the original
            // snapshot is stale by the time stop() returns. Re-snapshot once so
            // a quick disable->enable sequence does not leave the worker stopped
            // until some later settings change happens to reconcile again.
            if (computeTransition(snapshot(settings, worker)) == NotificationWorkerTransition::Start) {
                LOGI("Restarting unified worker because notification settings changed during stop");
                worker->begin();
            }
            break;
        case NotificationWorkerTransition::None:
        default:
            break;
    }
}

}  // namespace NOTIFICATIONS
