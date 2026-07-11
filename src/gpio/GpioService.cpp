#include "GpioService.h"

#include "../config/System.h"
#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"
#include "../system/utils/ScopeLock.h"
#include "GpioConfigStore.h"
#include "GpioSafePins.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "GpioService"

namespace GPIO {

namespace {
constexpr TickType_t kGpioLockTimeout = pdMS_TO_TICKS(100);
}

GpioService::GpioService(FS* fs)
    : _fs(fs),
      _mutex(xSemaphoreCreateMutex()) {}

GpioService::~GpioService() {
    stop();
    if (_mutex) {
        vSemaphoreDelete(_mutex);
        _mutex = nullptr;
    }
}

bool GpioService::begin() {
    GpioData config = CONFIG_STORE::copy();
    if (!normalizeConfig(config)) {
        LOGE("Refusing to start with invalid GPIO configuration");
        return false;
    }

    {
        SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
        if (!ensureLock(lock, "begin")) {
            return false;
        }
        applyConfigLocked(config, millis());
        LogicalChange initialLevels[kMaxChannels]{};
        const uint8_t initialLevelCount =
            snapshotLogicalLevelsLocked(initialLevels, kMaxChannels);
        dispatchChangesLocked(initialLevels, initialLevelCount);
    }

    _running = true;
    const BaseType_t created = xTaskCreatePinnedToCore(
        taskEntry,
        "gpio",
        CONFIG::TASKS::STACK_GPIO_TASK,
        this,
        CONFIG::TASKS::PRIO_GPIO_TASK,
        &_taskHandle,
        CONFIG::TASKS::CORE_GPIO_TASK);
    if (created != pdPASS) {
        _running = false;
        _taskHandle = nullptr;
        LOGE("Failed to start GPIO task");
        return false;
    }

    LOGI("GPIO service started (%u channels)", _channelCount);
    return true;
}

void GpioService::stop() {
    _running = false;
    if (_mutex) {
        // Callback execution is serialized by the same mutex, so this is also
        // an in-flight drain barrier before the task or mutex can disappear.
        (void)setInputChangeCallback(nullptr);
    }
    if (_taskHandle) {
        TaskHandle_t handle = _taskHandle;
        _taskHandle = nullptr;
        vTaskDelete(handle);
    }
}

bool GpioService::getConfig(GpioData& out) const {
    SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
    if (!ensureLock(lock, "getConfig")) {
        return false;
    }

    out = GpioData{};
    out.channelCount = _channelCount;
    for (uint8_t i = 0; i < _channelCount; i++) {
        out.channels[i] = _channels[i].config;
    }
    return true;
}

bool GpioService::getStatus(GpioChannelStatus* out, uint8_t maxItems, uint8_t& outCount) const {
    outCount = 0;
    if (!out || maxItems == 0) {
        return false;
    }

    SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
    if (!ensureLock(lock, "getStatus")) {
        return false;
    }

    const uint8_t count = _channelCount < maxItems ? _channelCount : maxItems;
    for (uint8_t i = 0; i < count; i++) {
        out[i].config = _channels[i].config;
        out[i].configured = _channels[i].configured;
        out[i].rawLevel = _channels[i].stableRawLevel;
        out[i].logicalLevel = _channels[i].logicalLevel;
        out[i].stable = _channels[i].stable;
        out[i].changedAtMs = _channels[i].changedAtMs;
        out[i].sampledAtMs = _channels[i].sampledAtMs;
    }
    outCount = count;
    return true;
}

bool GpioService::getChannelStatus(const char* id, GpioChannelStatus& out) const {
    SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
    if (!ensureLock(lock, "getChannelStatus")) {
        return false;
    }

    const int index = findChannelIndexLocked(id);
    if (index < 0) {
        return false;
    }

    const RuntimeChannel& channel = _channels[index];
    out.config = channel.config;
    out.configured = channel.configured;
    out.rawLevel = channel.stableRawLevel;
    out.logicalLevel = channel.logicalLevel;
    out.stable = channel.stable;
    out.changedAtMs = channel.changedAtMs;
    out.sampledAtMs = channel.sampledAtMs;
    return true;
}

bool GpioService::getLogicalValue(const char* id, bool& outValue) const {
    GpioChannelStatus status;
    if (!getChannelStatus(id, status) || status.config.mode == GpioMode::Disabled) {
        return false;
    }
    outValue = status.logicalLevel;
    return true;
}

bool GpioService::setInputChangeCallback(InputChangeCallback callback) {
    // GPIO callback work is deliberately restricted to AlarmService's fixed,
    // non-blocking mailbox. Serializing invocation with this mutex makes a
    // successful detach a true in-flight barrier without allocating a second
    // lifecycle primitive.
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    if (!ensureLock(lock, "setInputChangeCallback")) {
        return false;
    }
    _inputChangeCallback = std::move(callback);
    if (_inputChangeCallback) {
        LogicalChange levels[kMaxChannels]{};
        const uint8_t levelCount =
            snapshotLogicalLevelsLocked(levels, kMaxChannels);
        dispatchChangesLocked(levels, levelCount);
    }
    return true;
}

bool GpioService::updateConfig(const GpioData& nextConfig, bool persist) {
    GpioData normalized = nextConfig;
    if (!normalizeConfig(normalized)) {
        return false;
    }

    if (!CONFIG_STORE::update([&](GpioData& cfg) {
            cfg = normalized;
        })) {
        return false;
    }

    {
        SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
        if (!ensureLock(lock, "updateConfig")) {
            return false;
        }
        applyConfigAndPublishLocked(normalized, millis());
    }

    if (persist && !persistConfig()) {
        return false;
    }

    return true;
}

bool GpioService::setOutput(const char* id, bool logicalValue, bool persist) {
    bool changed = false;
    bool logicalChanged = false;
    LogicalChange outputChange{};
    {
        SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
        if (!ensureLock(lock, "setOutput")) {
            return false;
        }

        const int index = findChannelIndexLocked(id);
        if (index < 0) {
            return false;
        }

        RuntimeChannel& channel = _channels[index];
        if (channel.config.mode != GpioMode::Output) {
            return false;
        }

        logicalChanged = channel.logicalLevel != logicalValue;
        const bool raw = channel.config.inverted ? !logicalValue : logicalValue;
        digitalWrite(channel.config.pin, raw ? HIGH : LOW);
        channel.rawLevel = raw;
        channel.lastRawLevel = raw;
        channel.pendingRawLevel = raw;
        channel.stableRawLevel = raw;
        channel.logicalLevel = logicalValue;
        channel.stable = true;
        channel.sampledAtMs = millis();
        channel.changedAtMs = channel.sampledAtMs;
        channel.config.initialOutput = logicalValue;
        changed = true;
        if (logicalChanged) {
            strlcpy(outputChange.id, channel.config.id, sizeof(outputChange.id));
            outputChange.logicalValue = logicalValue;
            dispatchChangesLocked(&outputChange, 1);
        }
    }

    if (!changed) {
        return false;
    }

    const bool configUpdated = CONFIG_STORE::update([&](GpioData& cfg) {
        for (uint8_t i = 0; i < cfg.channelCount; i++) {
            if (strncmp(cfg.channels[i].id, id, kMaxIdLen) == 0) {
                cfg.channels[i].initialOutput = logicalValue;
                break;
            }
        }
    });

    if (!configUpdated) {
        return false;
    }

    return !persist || persistConfig();
}

void GpioService::taskEntry(void* arg) {
    auto* service = static_cast<GpioService*>(arg);
    while (service && service->_running) {
        service->sampleAndPublishInputs(millis());
        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
    }
    if (service) {
        service->_taskHandle = nullptr;
    }
    vTaskDelete(nullptr);
}

bool GpioService::ensureLock(const SYSTEM::ScopeLock& lock, const char* operation) const {
    if (lock.isLocked()) {
        return true;
    }

    LOGW("%s: mutex timeout", operation);
    return false;
}

bool GpioService::persistConfig() const {
    if (!_fs) {
        LOGW("persistConfig: filesystem unavailable");
        return false;
    }

    if (!CONFIG::save(*_fs)) {
        LOGE("Failed to persist GPIO config");
        return false;
    }
    return true;
}

void GpioService::applyConfigLocked(const GpioData& config, uint32_t nowMs) {
    _channelCount = config.channelCount > kMaxChannels ? kMaxChannels : config.channelCount;
    for (uint8_t i = 0; i < _channelCount; i++) {
        _channels[i] = RuntimeChannel{};
        _channels[i].config = config.channels[i];
        configureChannelLocked(_channels[i], nowMs);
    }
}

void GpioService::applyConfigAndPublishLocked(const GpioData& config,
                                              uint32_t nowMs) {
    LogicalChange oldActive[kMaxChannels]{};
    uint8_t oldActiveCount = 0;
    for (uint8_t i = 0; i < _channelCount && oldActiveCount < kMaxChannels; ++i) {
        if (_channels[i].config.mode == GpioMode::Disabled ||
            _channels[i].config.id[0] == '\0') {
            continue;
        }
        strlcpy(oldActive[oldActiveCount].id,
                _channels[i].config.id,
                sizeof(oldActive[oldActiveCount].id));
        ++oldActiveCount;
    }

    applyConfigLocked(config, nowMs);

    LogicalChange changes[kMaxChannels * 2]{};
    uint8_t changeCount = 0;
    for (uint8_t i = 0; i < oldActiveCount; ++i) {
        // A selector absent from the replacement snapshot has no new channel
        // from which to publish its state. Emit its terminal clear explicitly.
        // A retained-but-disabled selector is covered by the full new snapshot
        // below, which also publishes false.
        if (findChannelIndexLocked(oldActive[i].id) < 0) {
            strlcpy(changes[changeCount].id,
                    oldActive[i].id,
                    sizeof(changes[changeCount].id));
            changes[changeCount].logicalValue = false;
            ++changeCount;
        }
    }

    changeCount += snapshotLogicalLevelsLocked(
        changes + changeCount,
        static_cast<uint8_t>((kMaxChannels * 2) - changeCount));
    dispatchChangesLocked(changes, changeCount);
}

#ifdef UNIT_TEST
bool GpioService::applyRuntimeConfigForTest(const GpioData& config,
                                            uint32_t nowMs) {
    SYSTEM::ScopeLock lock(_mutex, portMAX_DELAY);
    if (!ensureLock(lock, "applyRuntimeConfigForTest")) {
        return false;
    }
    applyConfigAndPublishLocked(config, nowMs);
    return true;
}
#endif

void GpioService::configureChannelLocked(RuntimeChannel& channel, uint32_t nowMs) {
    channel.configured = false;
    channel.sampledAtMs = nowMs;
    channel.changedAtMs = nowMs;
    channel.rawChangedAtMs = nowMs;

    if (channel.config.mode == GpioMode::Disabled) {
        pinMode(channel.config.pin, INPUT);
        channel.stable = false;
        return;
    }

    pinMode(channel.config.pin, arduinoPinModeFor(channel.config));
    if (channel.config.mode == GpioMode::Output) {
        const bool raw = channel.config.inverted ? !channel.config.initialOutput : channel.config.initialOutput;
        digitalWrite(channel.config.pin, raw ? HIGH : LOW);
        channel.rawLevel = raw;
        channel.lastRawLevel = raw;
        channel.pendingRawLevel = raw;
        channel.stableRawLevel = raw;
        channel.logicalLevel = channel.config.initialOutput;
        channel.stable = true;
    } else {
        const bool raw = digitalRead(channel.config.pin) == HIGH;
        channel.rawLevel = raw;
        channel.lastRawLevel = raw;
        channel.pendingRawLevel = raw;
        channel.stableRawLevel = raw;
        channel.logicalLevel = logicalFromRaw(raw, channel.config.inverted);
        channel.stable = true;
    }
    channel.configured = true;
}

void GpioService::sampleAndPublishInputs(uint32_t nowMs) {
    LogicalChange changes[kMaxChannels]{};
    uint8_t changeCount = 0;
    SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
    if (!lock.isLocked()) {
        return;
    }
    sampleInputsLocked(nowMs, changes, changeCount);
    dispatchChangesLocked(changes, changeCount);
}

void GpioService::sampleInputsLocked(uint32_t nowMs,
                                     LogicalChange* changes,
                                     uint8_t& changeCount) {
    changeCount = 0;
    for (uint8_t i = 0; i < _channelCount; i++) {
        RuntimeChannel& channel = _channels[i];
        if (channel.config.mode != GpioMode::Input ||
            !sampleInputLocked(channel, nowMs)) {
            continue;
        }
        if (changes && changeCount < kMaxChannels) {
            LogicalChange& change = changes[changeCount++];
            strlcpy(change.id, channel.config.id, sizeof(change.id));
            change.logicalValue = channel.logicalLevel;
        }
    }
}

bool GpioService::sampleInputLocked(RuntimeChannel& channel, uint32_t nowMs) {
    const bool raw = digitalRead(channel.config.pin) == HIGH;
    channel.rawLevel = raw;
    channel.sampledAtMs = nowMs;

    if (raw != channel.pendingRawLevel) {
        channel.pendingRawLevel = raw;
        channel.rawChangedAtMs = nowMs;
        channel.stable = false;
        return false;
    }

    const uint32_t elapsed = nowMs - channel.rawChangedAtMs;
    if (raw != channel.stableRawLevel && elapsed >= channel.config.debounceMs) {
        channel.stableRawLevel = raw;
        channel.logicalLevel = logicalFromRaw(raw, channel.config.inverted);
        channel.changedAtMs = nowMs;
        channel.stable = true;
        return true;
    } else if (raw == channel.stableRawLevel && elapsed >= channel.config.debounceMs) {
        channel.stable = true;
    }
    return false;
}

uint8_t GpioService::snapshotLogicalLevelsLocked(LogicalChange* changes,
                                                 uint8_t maxChanges) const {
    if (!changes || maxChanges == 0) {
        return 0;
    }

    const uint8_t count = _channelCount < maxChanges ? _channelCount : maxChanges;
    for (uint8_t i = 0; i < count; ++i) {
        strlcpy(changes[i].id, _channels[i].config.id, sizeof(changes[i].id));
        changes[i].logicalValue =
            _channels[i].config.mode == GpioMode::Disabled
                ? false
                : _channels[i].logicalLevel;
    }
    return count;
}

void GpioService::dispatchChangesLocked(const LogicalChange* changes,
                                        uint8_t changeCount) {
    if (!_inputChangeCallback || !changes) {
        return;
    }
    for (uint8_t i = 0; i < changeCount; ++i) {
        if (changes[i].id[0] != '\0') {
            _inputChangeCallback(changes[i].id, changes[i].logicalValue);
        }
    }
}

int GpioService::findChannelIndexLocked(const char* id) const {
    if (!id || id[0] == '\0') {
        return -1;
    }

    for (uint8_t i = 0; i < _channelCount; i++) {
        if (strncmp(_channels[i].config.id, id, kMaxIdLen) == 0) {
            return i;
        }
    }
    return -1;
}

bool GpioService::logicalFromRaw(bool raw, bool inverted) {
    return inverted ? !raw : raw;
}

int GpioService::arduinoPinModeFor(const GpioChannelConfig& config) {
    if (config.mode == GpioMode::Output) {
        return OUTPUT;
    }
    if (config.pull == GpioPull::Up) {
        return INPUT_PULLUP;
    }
#if defined(INPUT_PULLDOWN)
    if (config.pull == GpioPull::Down) {
        return INPUT_PULLDOWN;
    }
#endif
    return INPUT;
}

}  // namespace GPIO
