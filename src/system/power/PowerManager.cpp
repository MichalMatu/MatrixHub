#include "PowerManager.h"
#include "../logging/Logging.h"
#include "../rtc/RtcConfig.h"
#include "../../config/App.h"


#undef LOG_TAG
#define LOG_TAG "Power"

namespace POWER {







uint32_t PowerManager::nowMs() {
    return _activityTracker.nowMs();
}

void PowerManager::begin() {
    _settings.begin();
    
    // Cache values for local use just for this log, or use getters directly
    uint32_t timeout = _settings.getInactivityTimeout();
    uint32_t grace = _settings.getGracePeriod();
    
    _wakeController.begin();
    _sleepController.begin(&_wakeController);
    _sleepController.setWakeConfig(_settings.getWakeSourceConfig());
    _activityTracker.begin(_wakeController.getWakeReason());

    WakeReason wakeReason = _wakeController.getWakeReason();
    LOGI("[Power] Sleep:%s T/O:%lu ms Grace:%lu ms | Wake:%lu ms Reason:%d Cause:%d Mask:0x%08llX",
         _settings.getSleepEnabled() ? "ON" : "OFF", (unsigned long)timeout, (unsigned long)grace, 
         (unsigned long)_sleepController.getWakeInterval(),
         (int)wakeReason, (int)_wakeController.getWakeupCauseRaw(),
         _wakeController.getGpioWakeupMask());
}

bool PowerManager::setInactivityTimeout(uint32_t timeoutMs) {
    return _settings.setInactivityTimeout(timeoutMs);
}

bool PowerManager::setGracePeriod(uint32_t graceMs) {
    return _settings.setGracePeriod(graceMs);
}

uint32_t PowerManager::getInactivityTimeout() const {
    return _settings.getInactivityTimeout();
}

uint32_t PowerManager::getGracePeriod() const {
    return _settings.getGracePeriod();
}

bool PowerManager::setSleepEnabled(bool enabled) {
    return _settings.setSleepEnabled(enabled);
}

bool PowerManager::getSleepEnabled() const {
    return _settings.getSleepEnabled();
}

bool PowerManager::applyConfig(const RTC::PowerData& config) {
    if (!_settings.applyConfig(config)) {
        return false;
    }

    _sleepController.setWakeConfig(_settings.getWakeSourceConfig());
    return true;
}

void PowerManager::notifyActivity(const char *source) {
    _activityTracker.notifyActivity(source);
}

void PowerManager::loopTick() {
    _sleepController.loopTick();

    if (_sleepController.isSleepRequested()) {
        return;
    }
    
    _activityTracker.loopTick(_sleepController, _settings);
}

void PowerManager::requestSleep(const char *reason, uint32_t delayMs) {
    _sleepController.requestSleep(reason, delayMs);
}

bool PowerManager::isSleepRequested() {
    return _sleepController.isSleepRequested();
}

WakeReason PowerManager::wakeReason() {
    return _wakeController.getWakeReason();
}

esp_sleep_wakeup_cause_t PowerManager::getWakeupCauseRaw() {
    return _wakeController.getWakeupCauseRaw();
}

uint64_t PowerManager::getGpioWakeupMask() {
    return _wakeController.getGpioWakeupMask();
}

uint64_t PowerManager::getExt1WakeupMask() {
    return _wakeController.getExt1WakeupMask();
}

InactivityConfig PowerManager::inactivityConfig() {
    return _settings.getInactivityConfig();
}

RTC::PowerData PowerManager::powerConfig() {
    return RTC::getConfig().power;
}

uint32_t PowerManager::lastActivityMs() {
    return _activityTracker.getLastActivityMs();
}

uint32_t PowerManager::bootMs() {
    return _activityTracker.getBootMs();
}

uint32_t PowerManager::wakeIntervalMs() {
    return _sleepController.getWakeInterval();
}

uint32_t PowerManager::sleepEtaMs() {
    return _sleepController.getSleepEtaMs();
}

uint32_t PowerManager::wakeAwakeWindowMs() {
    return _activityTracker.getWakeAwakeWindowMs(_settings);
}

uint32_t PowerManager::wakeAwakeEtaMs() {
    return _activityTracker.getWakeAwakeEtaMs(_settings);
}

void PowerManager::setPreSleepHook(void (*hook)()) {
    _sleepController.setPreSleepHook(hook);
}

void PowerManager::setTimeProvider(uint32_t (*provider)()) {
    _activityTracker.setTimeProvider(provider);
}

void PowerManager::setApStationsProvider(int (*provider)()) {
    _activityTracker.setApStationsProvider(provider);
}

void PowerManager::setSleepCallback(void (*callback)(const char *)) {
    _sleepController.setSleepCallback(callback);
}

void PowerManager::setConfigureWakeSourcesCallback(void (*callback)()) {
    _wakeController.setConfigureWakeSourcesCallback(callback);
}

void PowerManager::resetTestHooks() {
    _activityTracker.resetTestHooks();
    _wakeController.resetTestHooks();
    _sleepController.resetTestHooks();
}

void PowerManager::setWakeInterval(uint32_t intervalMs) {
    _sleepController.setWakeInterval(intervalMs);
}

const char* PowerManager::getSleepReason() {
    return _sleepController.getSleepReason();
}

}  // namespace POWER
