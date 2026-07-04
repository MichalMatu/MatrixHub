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
    normalizeConfig(config);

    {
        SYSTEM::ScopeLock lock(_mutex, kGpioLockTimeout);
        if (!ensureLock(lock, "begin")) {
            return false;
        }
        applyConfigLocked(config, millis());
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
        applyConfigLocked(normalized, millis());
    }

    if (persist && !persistConfig()) {
        return false;
    }

    return true;
}

bool GpioService::setOutput(const char* id, bool logicalValue, bool persist) {
    bool changed = false;
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
    }

    if (!changed) {
        return false;
    }

    if (!CONFIG_STORE::update([&](GpioData& cfg) {
            for (uint8_t i = 0; i < cfg.channelCount; i++) {
                if (strncmp(cfg.channels[i].id, id, kMaxIdLen) == 0) {
                    cfg.channels[i].initialOutput = logicalValue;
                    break;
                }
            }
        })) {
        return false;
    }

    return !persist || persistConfig();
}

void GpioService::taskEntry(void* arg) {
    auto* service = static_cast<GpioService*>(arg);
    while (service && service->_running) {
        {
            SYSTEM::ScopeLock lock(service->_mutex, kGpioLockTimeout);
            if (lock.isLocked()) {
                service->sampleInputsLocked(millis());
            }
        }
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

void GpioService::sampleInputsLocked(uint32_t nowMs) {
    for (uint8_t i = 0; i < _channelCount; i++) {
        if (_channels[i].config.mode == GpioMode::Input) {
            sampleInputLocked(_channels[i], nowMs);
        }
    }
}

void GpioService::sampleInputLocked(RuntimeChannel& channel, uint32_t nowMs) {
    const bool raw = digitalRead(channel.config.pin) == HIGH;
    channel.rawLevel = raw;
    channel.sampledAtMs = nowMs;

    if (raw != channel.pendingRawLevel) {
        channel.pendingRawLevel = raw;
        channel.rawChangedAtMs = nowMs;
        channel.stable = false;
        return;
    }

    const uint32_t elapsed = nowMs - channel.rawChangedAtMs;
    if (raw != channel.stableRawLevel && elapsed >= channel.config.debounceMs) {
        channel.stableRawLevel = raw;
        channel.logicalLevel = logicalFromRaw(raw, channel.config.inverted);
        channel.changedAtMs = nowMs;
        channel.stable = true;
    } else if (raw == channel.stableRawLevel && elapsed >= channel.config.debounceMs) {
        channel.stable = true;
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

