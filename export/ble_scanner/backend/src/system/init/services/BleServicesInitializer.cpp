/**
 * @file BleServicesInitializer.cpp
 * @brief BLE services initialization implementation
 */

#include "BleServicesInitializer.h"
#include "../../../ble/BleService.h"
#include "../../../ble/settings/BleSettingsService.h"
#include "../../logging/Logging.h"

#undef LOG_TAG
#define LOG_TAG "BleInit"

void BleServicesInitializer::initialize(
    BLE::BleSettingsService* bleSettings,
    BLE::BleService* bleService,
    const std::atomic<bool>* isDying,
    std::size_t& whitelistHandlerId) {
    if (!bleSettings || !bleService) {
        LOGW("Skipping BLE settings wiring due to missing dependencies");
        return;
    }

    whitelistHandlerId = 0;
    LOGI("BleSettingsService wired to runtime");

    BLE::BleConfig config;
    config.enabled = bleSettings->isEnabled();
    if (!bleService->updateConfig(config)) {
        LOGW("BleService runtime config could not be applied during boot");
    }

    bleSettings->setOnSettingsChanged([bleService, isDying](bool enabled) {
        if (isDying && isDying->load(std::memory_order_acquire)) {
            return true;
        }
        BLE::BleConfig cfg;
        cfg.enabled = enabled;
        return bleService->updateConfig(cfg);
    });

    whitelistHandlerId = bleSettings->addUpdateHandler([bleService, isDying](std::string_view originId) {
        (void)originId;
        if (isDying && isDying->load(std::memory_order_acquire)) {
            return StateHandlerResult::success();
        }
        bleService->refreshWhitelist();
        return StateHandlerResult::success();
    });

    LOGI("BleService runtime config applied (enabled=%s)", config.enabled ? "true" : "false");
}
