/**
 * @file ActivityPersistence.cpp
 * @brief Implementation of RTC memory persistence
 */

#include "ActivityPersistence.h"
#include <esp_attr.h>
#include "../../logging/Logging.h"
#include <esp_crc.h>

namespace POWER {

// RTC memory magic for validation
constexpr uint32_t ACTIVITY_RTC_MAGIC = 0xACF1D1E5;

// RTC memory storage (survives deep sleep and soft reboot)
RTC_DATA_ATTR ActivityPersistence _rtcActivity = {};

uint32_t ActivityPersistenceManager::calculateCrc(const ActivityPersistence& data) {
    // CRC over magic + lastActivityMs + bootMs (exclude crc32 field itself)
    return esp_crc32_le(0, (const uint8_t*)&data, offsetof(ActivityPersistence, crc32));
}

bool ActivityPersistenceManager::tryRestore(uint32_t& lastActivityMs, uint32_t& bootMs) {
    // Validate magic
    if (_rtcActivity.magic != ACTIVITY_RTC_MAGIC) {
        LOGD("[Power] RTC activity: invalid magic (cold boot or power loss)");
        return false;
    }
    
    // Validate CRC
    uint32_t expectedCrc = calculateCrc(_rtcActivity);
    if (_rtcActivity.crc32 != expectedCrc) {
        LOGW("[Power] RTC activity: CRC mismatch (corrupted data)");
        return false;
    }
    
    // Data valid - restore
    lastActivityMs = _rtcActivity.lastActivityMs;
    bootMs = _rtcActivity.bootMs;
    LOGI("[Power] RTC activity restored: lastActivity=%lu, boot=%lu", 
         lastActivityMs, bootMs);
    return true;
}

void ActivityPersistenceManager::save(uint32_t lastActivityMs, uint32_t bootMs) {
    _rtcActivity.magic = ACTIVITY_RTC_MAGIC;
    _rtcActivity.lastActivityMs = lastActivityMs;
    _rtcActivity.bootMs = bootMs;
    _rtcActivity.crc32 = calculateCrc(_rtcActivity);
}

}  // namespace POWER
