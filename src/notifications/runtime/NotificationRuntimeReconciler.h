#pragma once

class NotificationSettingsService;

namespace NOTIFICATIONS {

class NotificationWorker;

struct NotificationRuntimeSnapshot {
    bool settingsSnapshotValid = true;
    bool telegramEnabled = false;
    bool webhookEnabled = false;
    bool pushoverEnabled = false;
    bool workerRunning = false;
};

enum class NotificationWorkerTransition {
    None = 0,
    Start,
    Stop,
};

class NotificationRuntimeReconciler {
public:
    static bool shouldWorkerRun(const NotificationRuntimeSnapshot& snapshot) {
        if (!snapshot.settingsSnapshotValid) {
            // Keep current runtime state when config snapshot is temporarily unavailable.
            return snapshot.workerRunning;
        }
        return snapshot.telegramEnabled || snapshot.webhookEnabled || snapshot.pushoverEnabled;
    }

    static NotificationWorkerTransition computeTransition(const NotificationRuntimeSnapshot& snapshot) {
        const bool shouldRun = shouldWorkerRun(snapshot);
        if (shouldRun == snapshot.workerRunning) {
            return NotificationWorkerTransition::None;
        }
        return shouldRun ? NotificationWorkerTransition::Start : NotificationWorkerTransition::Stop;
    }

    static NotificationRuntimeSnapshot snapshot(const NotificationSettingsService* settings,
                                                const NotificationWorker* worker);
    static void reconcile(const NotificationSettingsService* settings, NotificationWorker* worker);
};

}  // namespace NOTIFICATIONS
