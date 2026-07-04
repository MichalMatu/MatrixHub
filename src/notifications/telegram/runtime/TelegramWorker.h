/**
 * @file TelegramWorker.h
 * @brief Unified Telegram polling and sending worker
 * 
 * Single FreeRTOS task that:
 * 1. Sends queued outbound messages
 * 2. Polls for incoming commands
 * 3. Dispatches commands and queues responses
 * 
 * Uses shared TelegramClient (one TLS connection for everything).
 */

#pragma once

#include <cstdint>
#include <functional>
#include "freertos/FreeRTOS.h"
#include <freertos/task.h>
#include "TelegramRuntimeConfigView.h"
#include "../../../config/System.h"

class NotificationSettingsService;

namespace ALARMS { class AlarmService; }
namespace MACROS { class MacroService; }
namespace MATRIX_MANAGER { class MatrixManagerService; }
namespace USB_TERMINAL { class UsbTerminalService; }
namespace SHELLY { class ShellyService; }
class ServiceRegistry;
class SecurityManager;

namespace TELEGRAM {

class TelegramClient;
class MessageQueue;
struct TelegramCommandRuntimeState;

struct TelegramWorkerServices {
    ALARMS::AlarmService* alarmService = nullptr;
    MACROS::MacroService* macroService = nullptr;
    MATRIX_MANAGER::MatrixManagerService* matrixManager = nullptr;
    SecurityManager* securityManager = nullptr;
    USB_TERMINAL::UsbTerminalService* usbTerminalService = nullptr;
    SHELLY::ShellyService* shellyService = nullptr;
    ServiceRegistry* registry = nullptr;
};

struct WorkerStatus {
    bool running = false;
    bool enabled = false;
    uint32_t lastPollMs = 0;
    uint32_t lastSendMs = 0;
    uint32_t messagesProcessed = 0;
    uint32_t messagesSent = 0;
    uint32_t messagesFailed = 0;
    uint32_t commandsExecuted = 0;
    int lastHttpCode = 0;
    char lastError[64] = {0};
};

using StatusChangeCallback = std::function<void(const WorkerStatus&)>;

class TelegramWorker {
public:
    // The worker now receives both outbound queue and inbound command scratch
    // state through DI. Target behavior: TelegramWorker coordinates runtime
    // flow, but does not hide ownership of long-lived Telegram buffers.
    TelegramWorker(
        TelegramClient* client,
        NotificationSettingsService* settings,
        MessageQueue* outboundQueue,
        TelegramCommandRuntimeState* commandRuntime,
        const TelegramWorkerServices& services);
    ~TelegramWorker() = default;

    /**
     * Start the worker task
     */
    void start();
    
    /**
     * Stop the worker task
     */
    void stop();
    
    /**
     * Check if worker is running
     */
    bool isRunning() const;
    
    /**
     * Get current status
     */
    WorkerStatus getStatus() const;

    /** Get RTC memory usage for static buffers (0; buffers are in PSRAM) */
    static size_t getRtcMemoryUsage();
    /** Get PSRAM usage for the injected Telegram command runtime scratch */
    static size_t getPsramMemoryUsage();

    /** Process outbound queue (returns true if work done) */
    bool processOutbound();

    /** Poll for commands (returns true if poll performed) */
    bool processInbound();

    /** Process one unified cycle of outbound + inbound work */
    bool processCycle();

    /** Register callback for status changes (called after outbound/inbound processing) */
    void setStatusChangeCallback(StatusChangeCallback cb) { _onStatusChange = std::move(cb); }

    /** Record a successful test message send (increments counter + fires WS callback) */
    void recordTestSend();

    /** Enqueue a message to the outbound queue (DI-friendly interface) */
    bool enqueue(const char* chatId, const char* text);

private:
    void initializeConfigSnapshot();
    void syncConfigChanges();
    void resetPollingState(bool resetUpdateOffset, bool resetTransport, bool enabled, bool commandsEnabled);

    TelegramClient* _client = nullptr;
    NotificationSettingsService* _settings = nullptr;
    MessageQueue* _queue = nullptr;
    // Shared PSRAM scratch/runtime for inbound command polling/dispatch. This
    // is borrowed from ServiceRegistry and reused across polls to avoid per-loop
    // heap churn while keeping ownership explicit.
    TelegramCommandRuntimeState* _commandRuntime = nullptr;
    
    WorkerStatus _status;
    SemaphoreHandle_t _statusMutex = nullptr;
    uint32_t _nextOutboundSendMs = 0;
    uint32_t _currentJitterMs = 0;
    uint32_t _consecutiveErrors = 0;
    bool _configSnapshotInitialized = false;
    bool _lastTelegramEnabled = false;
    bool _lastCommandsEnabled = false;
    char _lastBotToken[64] = {0};
    StatusChangeCallback _onStatusChange;
    
    // static constexpr uint32_t kStackBytes = CONFIG::TASKS::STACK_TELEGRAM; // Moved to NotificationWorker
    // static constexpr UBaseType_t kPriority = CONFIG::TASKS::PRIO_BACKGROUND;
};

}  // namespace TELEGRAM
