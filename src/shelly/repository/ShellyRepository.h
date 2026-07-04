#pragma once

#include <vector>
#include <FS.h>
#include "../ShellyTypes.h"

namespace RTC {
    struct ShellyData;
}

namespace SHELLY {

/**
 * Repository for Shelly device configuration persistence.
 * 
 * Handles loading and saving device configuration to/from filesystem.
 * Uses JSON format for storage.
 */
class ShellyRepository {
public:
    /**
     * Construct repository with filesystem reference.
     * 
     * @param fs Reference to filesystem (e.g., LittleFS)
     */
    explicit ShellyRepository(FS& fs);
    
    ~ShellyRepository() = default;

    /**
     * Load device configuration from filesystem.
     * 
     * @param devices Vector to populate with loaded devices
     * @return true if successful (or file doesn't exist), false on error
     */
    bool load(std::vector<ShellyDevice>& devices);

    /**
     * Save device configuration to filesystem.
     * 
     * @param devices Vector of devices to save
     * @return true if successful, false on error
     */
    bool save(const std::vector<ShellyDevice>& devices);

    /**
     * Check if configuration file exists.
     * 
     * @return true if file exists, false otherwise
     */
    bool exists() const;

    /**
     * Delete configuration file.
     * 
     * @return true if deleted or didn't exist, false on error
     */
    bool remove();

    /**
     * Get filesystem reference.
     */
    FS& getFs() { return _fs; }

private:
    FS& _fs;
};

} // namespace SHELLY
