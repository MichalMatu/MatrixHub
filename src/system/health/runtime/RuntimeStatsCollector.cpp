/**
 * @file RuntimeStatsCollector.cpp
 * @brief Implementation of runtime statistics collection
 */

#include "RuntimeStatsCollector.h"
#include "../../logging/Logging.h"
#include <cstring>

#undef LOG_TAG
#define LOG_TAG "RuntimeStats"

namespace SYSTEM {
namespace HEALTH {

// Static member initialization
RuntimeStats RuntimeStatsCollector::_stats = {};

void RuntimeStatsCollector::begin() {
    memset(&_stats, 0, sizeof(_stats));
    _stats.bootMs = millis();
    _stats.lastLoopMs = _stats.bootMs;
    
    LOGD("Runtime stats collector initialized");
}

void RuntimeStatsCollector::updateLoop() {
    _stats.loopCount++;
    _stats.lastLoopMs = millis();
}

void RuntimeStatsCollector::recordSlowLoop() {
    _stats.slowLoopCount++;
}

void RuntimeStatsCollector::recordHttpRequest() {
    _stats.httpRequestCount++;
}

void RuntimeStatsCollector::recordTelegramPoll() {
    _stats.telegramPollCount++;
}

void RuntimeStatsCollector::recordSensorRead() {
    _stats.sensorReadCount++;
}

void RuntimeStatsCollector::recordFsWrite() {
    _stats.fsWriteCount++;
}

const RuntimeStats& RuntimeStatsCollector::getStats() {
    return _stats;
}

uint32_t RuntimeStatsCollector::getUptimeMs() {
    return millis() - _stats.bootMs;
}

}  // namespace HEALTH
}  // namespace SYSTEM
