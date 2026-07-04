#include "TelegramWorker.h"
#include "../client/TelegramClient.h"
#include "TelegramCommandRuntimeState.h"
#include "TelegramRuntimeConfigView.h"
#include "../queue/MessageQueue.h"
#include "../polling/TelegramPoller.h"
#include "../queue/TelegramQueueProcessor.h"
#include "../commands/TelegramCommandParser.h"
#include "../utils/BackoffCalculator.h"

#include "../commands/TelegramCommandDispatcher.h"
#include "../../settings/NotificationSettingsService.h"
#include "../client/TelegramConnectionValidator.h"
#include "../../../system/logging/Logging.h"
#include "../../../system/health/heap/HeapMonitor.h"
#include "../../../system/rtc/RtcConfig.h"
#include "../../../system/utils/Random.h"
#include "../../../config/App.h"
#include "../../../system/utils/ScopeLock.h"

#include <WiFi.h>
#include <time.h>

#undef LOG_TAG
#define LOG_TAG "TgWorker"

namespace TELEGRAM {

namespace {
bool isDelayActive(uint32_t nowMs, uint32_t untilMs) {
    return untilMs != 0 && static_cast<int32_t>(untilMs - nowMs) > 0;
}
}

TelegramWorker::TelegramWorker(
    TelegramClient* client,
    NotificationSettingsService* settings,
    MessageQueue* outboundQueue,
    TelegramCommandRuntimeState* commandRuntime,
    const TelegramWorkerServices& services)
    : _client(client)
    , _settings(settings)
    , _queue(outboundQueue)
    , _commandRuntime(commandRuntime) {
    
    if (!_statusMutex) {
        _statusMutex = xSemaphoreCreateBinary();
        if (_statusMutex) xSemaphoreGive(_statusMutex);
    }
    
    if (_commandRuntime) {
        // The command scratch/runtime buffer is registry-owned now, but the
        // worker still seeds its long-lived service references here so the
        // command handlers keep using the same context contract as before.
        // Target behavior: command handlers stay unchanged, while ownership of
        // the backing state moves out of TelegramWorker and into the registry.
        _commandRuntime->commandContext.alarmService = services.alarmService;
        _commandRuntime->commandContext.macroService = services.macroService;
        _commandRuntime->commandContext.matrixManager = services.matrixManager;
        _commandRuntime->commandContext.securityManager = services.securityManager;
        _commandRuntime->commandContext.usbTerminalService = services.usbTerminalService;
        _commandRuntime->commandContext.shellyService = services.shellyService;
        _commandRuntime->commandContext.registry = services.registry;
    }

    initializeConfigSnapshot();
    
    LOGI("Initialized (lastUpdateId=%lld from RTC)", RTC::runtimeStats.telegramLastUpdateId);
}

size_t TelegramWorker::getRtcMemoryUsage() {
    // Command scratch is now registry-owned PSRAM memory, so the worker no
    // longer carries hidden RTC/static allocations.
    return 0;
}

size_t TelegramWorker::getPsramMemoryUsage() {
    // Keep diagnostics honest after the refactor: the command runtime still
    // exists, but it now lives in PSRAM under explicit registry ownership.
    return sizeof(TelegramCommandRuntimeState);
}

// "Start" now just means "Enabled" logically, no task creation
void TelegramWorker::start() {
    if (!_settings) {
        LOGE("Not initialized");
        return;
    }
    if (_statusMutex) {
        SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
        if (lock.isLocked()) {
            _status.running = true;
            _status.enabled = true;
        }
    } else {
        _status.running = true;
        _status.enabled = true;
    }
    LOGI("Enabled (Managed by NotificationWorker)");
}

void TelegramWorker::stop() {
    LOGI("Stopping logic...");
    if (_statusMutex) {
        SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
        if (lock.isLocked()) {
            _status.running = false;
            return;
        }
    }
    _status.running = false;
}

bool TelegramWorker::isRunning() const {
    return _status.running;
}

WorkerStatus TelegramWorker::getStatus() const {
    WorkerStatus copy = {};
    if (_statusMutex) {
        SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
        if (lock.isLocked()) {
            copy = _status;
        }
    }
    return copy;
}

void TelegramWorker::recordTestSend() {
    if (_statusMutex) {
        SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
        if (lock.isLocked()) {
            _status.messagesSent++;
            _status.lastSendMs = millis();
        }
    } else {
        // Fallback if no mutex (should not happen post-init)
        _status.messagesSent++;
        _status.lastSendMs = millis();
    }
    
    RTC::runtimeStats.telegramMsgsSent++;
    
    // The _onStatusChange callback triggers a WebSocket broadcast.
    // However, recordTestSend() is often called from an AsyncHTTP request handler context (e.g. API test endpoint).
    // In ESP-IDF, calling lwIP networking functions (like DNS resolution in TelegramConnectionValidator
    // which might be triggered by the main loop or broadcaster) from the AsyncHTTP thread can cause 
    // a `sys_untimeout` panic because the thread doesn't hold the TCPIP core lock.
    // Instead of firing the callback here, we let the regular NotificationWorker loop detect
    // the status change and broadcast it safely from the main task context.
}

bool TelegramWorker::enqueue(const char* chatId, const char* text) {
    return _queue && _queue->enqueue(chatId, text);
}

void TelegramWorker::initializeConfigSnapshot() {
    const auto config = readTelegramRuntimeConfig(_settings);
    strlcpy(_lastBotToken, config.botToken, sizeof(_lastBotToken));
    _lastTelegramEnabled = config.enabled;
    _lastCommandsEnabled = config.commandsEnabled;
    _configSnapshotInitialized = true;
}

void TelegramWorker::resetPollingState(bool resetUpdateOffset, bool resetTransport, bool enabled, bool commandsEnabled) {
    _consecutiveErrors = 0;
    _currentJitterMs = 0;
    _nextOutboundSendMs = 0;

    if (resetUpdateOffset) {
        RTC::runtimeStats.telegramLastUpdateId = 0;
    }

    if (resetTransport && _client) {
        _client->resetSession();
    }

    if (_statusMutex) {
        SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
        if (lock.isLocked()) {
            _status.lastPollMs = 0;
            _status.lastHttpCode = 0;
            _status.lastError[0] = '\0';
            _status.running = enabled;
            _status.enabled = commandsEnabled;
            return;
        }
    }

    _status.lastPollMs = 0;
    _status.lastHttpCode = 0;
    _status.lastError[0] = '\0';
    _status.running = enabled;
    _status.enabled = commandsEnabled;
}

void TelegramWorker::syncConfigChanges() {
    if (!_configSnapshotInitialized) {
        initializeConfigSnapshot();
        return;
    }

    const auto config = readTelegramRuntimeConfig(_settings);
    const char* tokenSafe = config.botToken;
    const bool enabled = config.enabled;
    const bool commandsEnabled = config.commandsEnabled;

    const bool tokenChanged = strncmp(_lastBotToken, tokenSafe, sizeof(_lastBotToken)) != 0;
    const bool enabledChanged = _lastTelegramEnabled != enabled;
    const bool commandsChanged = _lastCommandsEnabled != commandsEnabled;

    if (!tokenChanged && !enabledChanged && !commandsChanged) {
        return;
    }

    LOGI("Config changed: token=%d enabled=%d->%d commands=%d->%d",
         tokenChanged ? 1 : 0,
         _lastTelegramEnabled ? 1 : 0,
         enabled ? 1 : 0,
         _lastCommandsEnabled ? 1 : 0,
         commandsEnabled ? 1 : 0);

    // Token changes must reset the update offset, otherwise a new bot inherits
    // the old bot's getUpdates offset and chat ID autodiscovery can stall forever.
    resetPollingState(tokenChanged, tokenChanged || enabledChanged, enabled, commandsEnabled);

    strlcpy(_lastBotToken, tokenSafe, sizeof(_lastBotToken));
    _lastTelegramEnabled = enabled;
    _lastCommandsEnabled = commandsEnabled;
}

bool TelegramWorker::processOutbound() {
    syncConfigChanges();

    const auto config = readTelegramRuntimeConfig(_settings);
    if (!config.configured) {
        return false;
    }
    
    // Check connectivity - quick check before processing
    if (!WiFi.isConnected()) return false;

    const size_t pendingCount = _queue ? _queue->count() : 0;
    if (pendingCount == 0) {
        _nextOutboundSendMs = 0;
        return false;
    }

    const uint32_t nowMs = millis();
    if (isDelayActive(nowMs, _nextOutboundSendMs)) {
        return false;
    }

    const OutboundProcessResult result =
        TelegramQueueProcessor::processOneQueuedMessage(_status, _client, config, _statusMutex, _queue);
    if (!result.didWork) {
        return false;
    }

    if (result.applyQueueDelay && _queue && _queue->count() > 0) {
        _nextOutboundSendMs = millis() + APP::NOTIFICATIONS::TELEGRAM_QUEUE_SEND_DELAY_MS;
    } else {
        _nextOutboundSendMs = 0;
    }

    if (_onStatusChange) _onStatusChange(getStatus());
    return true;
}

bool TelegramWorker::processInbound() {
    syncConfigChanges();

    const auto config = readTelegramRuntimeConfig(_settings);
    if (!config.configured) {
        return false;
    }

    const bool cmdEnabled = config.commandsEnabled;
    const bool needsDiscovery = !config.hasChatId;

    // Update status flags from live settings
    WorkerStatus statusSnapshot = {};
    if (_statusMutex) {
        SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
        if (!lock.isLocked()) {
            return false;
        }
        _status.enabled = cmdEnabled;
        statusSnapshot = _status;
    } else {
        _status.enabled = cmdEnabled;
        statusSnapshot = _status;
    }

    if (cmdEnabled || needsDiscovery) {
        if (!_commandRuntime) {
            // This should never happen after successful initialization. If it
            // does, fail closed and skip inbound work instead of touching an
            // invalid shared buffer.
            LOGE("Command runtime unavailable, skipping Telegram inbound work");
            return false;
        }
        if (_statusMutex) {
            SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
            if (lock.isLocked()) {
                _status.running = true;
            }
        } else {
            _status.running = true;
        }
        // Jitter logic (instance member to avoid shared state)
        // Backoff: consecutive errors increase the polling interval
        
        // Calculate effective interval with exponential backoff
        uint32_t baseInterval = APP::NOTIFICATIONS::TELEGRAM_POLL_INTERVAL_MS;
        uint32_t backoffMs = BackoffCalculator::calculateMs(_consecutiveErrors, baseInterval);
        uint32_t effectiveInterval = baseInterval + backoffMs + _currentJitterMs;
        
        uint32_t now = millis();
        if (now - statusSnapshot.lastPollMs >= effectiveInterval) {
            char discoveredChatId[TelegramRuntimeConfigView::kChatIdCapacity] = {0};
            
            // Update lastPollMs BEFORE the poll attempt to ensure
            // the interval is respected even on failure (prevents ~1s spam)
            if (_statusMutex) {
                SYSTEM::ScopeLock lock(_statusMutex, pdMS_TO_TICKS(50));
                if (lock.isLocked()) {
                    _status.lastPollMs = now;
                }
            } else {
                _status.lastPollMs = now;
            }
            
            // Check connectivity before expensive poll
             const char* offlineErr = nullptr;
             if (!NOTIFICATIONS::TELEGRAM::TelegramConnectionValidator::ensureOnline(
                     &offlineErr, APP::NOTIFICATIONS::TELEGRAM_TIME_WAIT_MS)) {
                 return false; 
             }

            TelegramPoller::pollAndDispatch(
                RTC::runtimeStats.telegramLastUpdateId,
                _status,
                config,
                _client,
                _statusMutex,
                _queue,
                // Poller/dispatcher still operate on one shared scratch object,
                // but that object is now injected explicitly instead of being a
                // hidden global in this translation unit.
                *_commandRuntime,
                discoveredChatId,
                sizeof(discoveredChatId));

            // Persist the auto-discovered first chat immediately so the pairing
            // model stays "one private owner chat" across reboots. This is tied
            // to the accepted low-risk onboarding window documented in the poller.
            if (!config.hasChatId && discoveredChatId[0] != '\0' && _settings) {
                _settings->setChatId(discoveredChatId);
            }
            
            // Track consecutive errors for backoff
            // HTTPC_ERROR_READ_TIMEOUT (-3) is expected in Long-Polling and should NOT trigger backoff
            if ((_status.lastHttpCode >= 200 && _status.lastHttpCode < 400) || 
                _status.lastHttpCode == HTTPC_ERROR_READ_TIMEOUT) {
                
                if (_consecutiveErrors > 0) {
                    LOGI("Poll recovered after %u consecutive errors", _consecutiveErrors);
                }
                _consecutiveErrors = 0;
            } else {
                _consecutiveErrors++;
                if (_consecutiveErrors <= 3 || (_consecutiveErrors % 10) == 0) {
                    uint32_t nextBackoff = BackoffCalculator::calculateMs(_consecutiveErrors, baseInterval);
                    LOGW("Poll error #%u (http=%d), next retry in %ums",
                         _consecutiveErrors, _status.lastHttpCode,
                         baseInterval + nextBackoff);
                }
            }
            
            // Pick new jitter for next cycle
            _currentJitterMs = UTILS::RNG::rangeU32Exclusive(
                APP::NOTIFICATIONS::TELEGRAM_POLL_JITTER_MS);
            if (_onStatusChange) _onStatusChange(getStatus());
            return true;
        }
    }
    return false;
}

bool TelegramWorker::processCycle() {
    bool didWork = false;

    if (processOutbound()) {
        didWork = true;
    }

    if (processInbound()) {
        didWork = true;
    }

    return didWork;
}

}  // namespace TELEGRAM
