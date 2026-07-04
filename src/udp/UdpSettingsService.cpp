/**
 * @file UdpSettingsService.cpp
 * @brief RTC-backed transactional settings service for UDP pusher config
 */

#include "UdpSettingsService.h"

#include "../core/config/ConfigManager.h"
#include "../system/logging/Logging.h"

#include <cstring>

#undef LOG_TAG
#define LOG_TAG "UdpSettings"

namespace UDPPUSH {

UdpSettingsService::UdpSettingsService(FS* fs)
    : RtcStatefulService(&RTC::ConfigStore::udpPusher),
      _fs(fs) {
    addUpdateHandler(
        [this](std::string_view originId) {
            (void)originId;
            return persistConfig();
        },
        false);
}

void UdpSettingsService::readState(RTC::UdpPusherData& settings, JsonObject& root) {
    root[CONFIG::Keys::kEnabled] = settings.enabled;
    root[CONFIG::Keys::kHost].set(String(settings.host));
    root[CONFIG::Keys::kPort] = settings.port;
    root[CONFIG::Keys::kFormat] = formatToString(settings.format);
    root[CONFIG::Keys::kIntervalMs] = settings.intervalMs;
}

StateUpdateResult UdpSettingsService::updateState(
    JsonObject& jsonObject,
    RTC::UdpPusherData& settings,
    std::string_view originId) {
    (void)originId;

    RTC::UdpPusherData nextState = settings;
    CONFIG::JSON::deserializeUdpPusher(jsonObject, nextState);

    if (memcmp(&settings, &nextState, sizeof(RTC::UdpPusherData)) == 0) {
        return StateUpdateResult::UNCHANGED;
    }

    settings = nextState;
    return StateUpdateResult::CHANGED;
}

const char* UdpSettingsService::formatToString(RTC::UdpFormat format) {
    switch (format) {
        case RTC::UdpFormat::Json:
            return "json";
        case RTC::UdpFormat::Csv:
            return "csv";
        default:
            return "line";
    }
}

StateHandlerResult UdpSettingsService::persistConfig() {
    if (!_fs || !CONFIG::save(*_fs)) {
        LOGE("Failed to persist UDP settings");
        return StateHandlerResult::failure("config/save_failed");
    }

    return StateHandlerResult::success();
}

}  // namespace UDPPUSH
