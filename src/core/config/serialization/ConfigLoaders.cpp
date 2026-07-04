#include "core/config/serialization/ConfigLoaders.h"

#include "config/json/AirMouseConfigJson.h"
#include "config/json/AlarmConfigJson.h"
#include "config/json/BleConfigJson.h"
#include "config/json/CompensationConfigJson.h"
#include "config/json/ConfigKeys.h"
#include "config/json/GpioConfigJson.h"
#include "config/json/KeyboardConfigJson.h"
#include "config/json/ImuConfigJson.h"
#include "config/json/MacroConfigJson.h"
#include "config/json/MatrixConfigJson.h"
#include "config/json/NotificationSettingsJson.h"
#include "config/json/PowerSettingsJson.h"
#include "config/json/ShellyConfigJson.h"
#include "config/json/SystemConfigJson.h"
#include "config/json/UsbTerminalConfigJson.h"
#include "config/json/WifiSensingConfigJson.h"
#include "system/memory/PsramAllocator.h"
#include <ArduinoJson.h>

namespace {

template <typename LoaderFn>
void loadIfObject(SYSTEM::SpiRamJsonDocument& doc, const char* key, LoaderFn loader) {
    if (!doc[key].is<JsonObject>()) {
        return;
    }
    JsonObject obj = doc[key].as<JsonObject>();
    loader(obj);
}

}  // namespace

namespace CONFIG::Serialization {

void loadConfigSections(SYSTEM::SpiRamJsonDocument& doc) {
    loadIfObject(doc, Keys::kNotification, JSON::loadNotification);
    loadIfObject(doc, Keys::kWifiSensing, JSON::loadWifiSensing);
    loadIfObject(doc, Keys::kBle, JSON::loadBle);
    loadIfObject(doc, Keys::kShelly, JSON::loadShelly);
    loadIfObject(doc, Keys::kAlarms, JSON::loadAlarms);
    loadIfObject(doc, Keys::kHeartbeat, JSON::loadHeartbeat);
    loadIfObject(doc, Keys::kUdpPusher, JSON::loadUdpPusher);
    loadIfObject(doc, Keys::kAirMouse, JSON::loadAirMouse);
    loadIfObject(doc, Keys::kImu, JSON::loadImu);
    loadIfObject(doc, Keys::kMatrix, JSON::loadMatrix);
    loadIfObject(doc, Keys::kMacros, JSON::loadMacros);
    loadIfObject(doc, Keys::kKeyboard, JSON::loadKeyboard);
    loadIfObject(doc, Keys::kLogging, JSON::loadLogging);
    loadIfObject(doc, Keys::kPower, JSON::loadPower);
    loadIfObject(doc, Keys::kCompensation, JSON::loadCompensation);
    loadIfObject(doc, Keys::kUsbTerminal, JSON::loadUsbTerminal);
    loadIfObject(doc, Keys::kGpio, JSON::loadGpio);
}

void loadPsramOnlyConfigSections(SYSTEM::SpiRamJsonDocument& doc) {
    loadIfObject(doc, Keys::kMatrix, JSON::loadMatrixPsram);
    loadIfObject(doc, Keys::kNotification, JSON::loadNotification);
    loadIfObject(doc, Keys::kShelly, JSON::loadShelly);
    loadIfObject(doc, Keys::kAlarms, JSON::loadAlarms);
    loadIfObject(doc, Keys::kHeartbeat, JSON::loadHeartbeat);
    loadIfObject(doc, Keys::kGpio, JSON::loadGpio);
}

}  // namespace CONFIG::Serialization
