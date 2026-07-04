/**
 * @file ActivityPersistence.h
 * @brief RTC memory persistence for activity tracking
 */

#pragma once

#include <cstdint>

namespace POWER {

// RTC memory structure for activity persistence (survives deep sleep & soft reboot)
struct ActivityPersistence {
    uint32_t magic;           // Validation magic number
    uint32_t lastActivityMs;  // millis() at last activity
    uint32_t bootMs;          // millis() at boot time
    uint32_t crc32;           // CRC32 checksum
};

/**
 * RTC memory persistence manager
 * 
 * Handles save/restore/validate of activity data in RTC memory.
 * RTC memory survives deep sleep and soft reboots (but not power loss).
 */
class ActivityPersistenceManager {
public:
    /**
     * Try to restore activity from RTC memory
     * 
     * @param lastActivityMs Output: restored last activity timestamp
     * @param bootMs Output: restored boot timestamp
     * @return true if data was valid and restored
     */
    static bool tryRestore(uint32_t& lastActivityMs, uint32_t& bootMs);
    
    /**
     * Save activity to RTC memory
     * 
     * @param lastActivityMs Last activity timestamp
     * @param bootMs Boot timestamp
     */
    static void save(uint32_t lastActivityMs, uint32_t bootMs);

private:
    ActivityPersistenceManager() = delete; // Static-only
    
    static uint32_t calculateCrc(const ActivityPersistence& data);
};

}  // namespace POWER
