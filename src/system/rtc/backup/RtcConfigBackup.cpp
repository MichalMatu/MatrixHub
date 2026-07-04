/**
 * @file RtcConfigBackup.cpp
 * @brief RTC retained backup snapshot and integrity helpers
 */

#include "../RtcConfigInternal.h"

#include "../../logging/Logging.h"

#include <esp_attr.h>
#include <esp_rom_crc.h>

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "RtcConfig"

namespace RTC {
namespace detail {
namespace {

constexpr uint32_t kBackupSlotMagic = 0xBAAC0F1C;
constexpr uint32_t kBackupPreCanary = 0xA55A1EAD;
constexpr uint32_t kBackupPostCanary = 0xC001D00D;

struct RetainedBackupSlot {
    uint32_t slotMagic;
    uint32_t generation;
    uint32_t mirroredStoreCrc;
    uint32_t preCanary;
    alignas(4) uint8_t configBytes[sizeof(ConfigStore)];
    uint32_t postCanary;
};

struct BackupValidationResult {
    bool valid = false;
    uint32_t calcCrc = 0;
};

RTC_DATA_ATTR RetainedBackupSlot sBackupStore;

uint32_t calculateConfigCrc(const ConfigStore& cfg) {
    const uint8_t* data = reinterpret_cast<const uint8_t*>(&cfg) + ConfigStore::kHeaderSize;
    const size_t len = sizeof(ConfigStore) - ConfigStore::kHeaderSize;
    return esp_rom_crc32_le(0, data, len);
}

const ConfigStore& slotConfig(const RetainedBackupSlot& slot) {
    return *reinterpret_cast<const ConfigStore*>(slot.configBytes);
}

BackupValidationResult validateBackupSlot(const RetainedBackupSlot& slot, bool logIfInvalid) {
    BackupValidationResult result{};

    if (slot.slotMagic != kBackupSlotMagic) {
        if (logIfInvalid) {
            LOGW("RTC backup invalid: slotMagic=0x%08X expected=0x%08X",
                 slot.slotMagic,
                 kBackupSlotMagic);
        }
        return result;
    }

    if (slot.preCanary != kBackupPreCanary || slot.postCanary != kBackupPostCanary) {
        if (logIfInvalid) {
            LOGW("RTC backup invalid: canary pre=0x%08X post=0x%08X",
                 slot.preCanary,
                 slot.postCanary);
        }
        return result;
    }

    if (slotConfig(slot).magic != kMagicValid) {
        if (logIfInvalid) {
            LOGW("RTC backup invalid: config.magic=0x%08X expected=0x%08X",
                 slotConfig(slot).magic,
                 kMagicValid);
        }
        return result;
    }

    if (slotConfig(slot).version != kSchemaVersion) {
        if (logIfInvalid) {
            LOGW("RTC backup invalid: version=%u expected=%u",
                 slotConfig(slot).version,
                 kSchemaVersion);
        }
        return result;
    }

    result.calcCrc = calculateConfigCrc(slotConfig(slot));
    if (slotConfig(slot).crc32 != result.calcCrc || slot.mirroredStoreCrc != result.calcCrc) {
        if (logIfInvalid) {
            LOGW("RTC backup invalid: cfg.crc=0x%08X mirror.crc=0x%08X calc=0x%08X gen=%lu",
                 slotConfig(slot).crc32,
                 slot.mirroredStoreCrc,
                 result.calcCrc,
                 static_cast<unsigned long>(slot.generation));
        }
        return result;
    }

    result.valid = true;
    return result;
}

uint32_t nextBackupGeneration() {
    const BackupValidationResult current = validateBackupSlot(sBackupStore, false);
    if (!current.valid) {
        return 1;
    }
    return sBackupStore.generation + 1;
}

void saveBackupSlot(RetainedBackupSlot& slot, uint32_t generation, const ConfigStore& cfg) {
    slot.slotMagic = 0;
    slot.generation = generation;
    slot.preCanary = kBackupPreCanary;
    memcpy(slot.configBytes, &cfg, sizeof(ConfigStore));
    slot.mirroredStoreCrc = slotConfig(slot).crc32;
    slot.postCanary = kBackupPostCanary;
    slot.slotMagic = kBackupSlotMagic;
}

bool isStoreValid(const ConfigStore& cfg) {
    if (cfg.magic != kMagicValid || cfg.version != kSchemaVersion) {
        return false;
    }
    return cfg.crc32 == calculateConfigCrc(cfg);
}

}  // namespace

void refreshConfigIntegrity(ConfigStore& cfg) {
    cfg.magic = kMagicValid;
    cfg.version = kSchemaVersion;
    cfg.crc32 = calculateConfigCrc(cfg);
}

bool restoreConfigFromBackup(ConfigStore& cfg) {
    const BackupValidationResult backup = validateBackupSlot(sBackupStore, true);
    if (!backup.valid) {
        LOGW("RTC Backup invalid/empty. Initializing defaults.");
        return false;
    }

    LOGI("Restoring RTC Config from backup (gen=%lu crc=0x%08X)",
         static_cast<unsigned long>(sBackupStore.generation),
         slotConfig(sBackupStore).crc32);
    memcpy(&cfg, &slotConfig(sBackupStore), sizeof(ConfigStore));
    return true;
}

bool saveConfigBackupSnapshot(const ConfigStore& cfg) {
    const uint32_t generation = nextBackupGeneration();
    saveBackupSlot(sBackupStore, generation, cfg);
    LOGI("RTC backup snapshot saved (gen=%lu crc=0x%08X)",
         static_cast<unsigned long>(generation),
         cfg.crc32);
    return true;
}

void invalidateConfigBackupSnapshot() {
    memset(&sBackupStore, 0, sizeof(sBackupStore));
}

}  // namespace detail

bool isValid() {
    return detail::isStoreValid(detail::requireStore());
}

void markValid() {
    detail::refreshConfigIntegrity(detail::requireStore());
}

}  // namespace RTC
