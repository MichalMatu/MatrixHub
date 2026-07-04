#include "core/config/serialization/ConfigSavers.h"

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

namespace CONFIG::Serialization {

void buildConfigDocument(SYSTEM::SpiRamJsonDocument& doc,
                         const ALARMS::AlarmRulesSnapshot* alarmRulesOverride) {
    JsonObject notification = doc[Keys::kNotification].to<JsonObject>();
    JSON::saveNotification(notification);

    JsonObject wifiSensing = doc[Keys::kWifiSensing].to<JsonObject>();
    JSON::saveWifiSensing(wifiSensing);

    JsonObject ble = doc[Keys::kBle].to<JsonObject>();
    JSON::saveBle(ble);

    JsonObject shelly = doc[Keys::kShelly].to<JsonObject>();
    JSON::saveShelly(shelly);

    JsonObject alarms = doc[Keys::kAlarms].to<JsonObject>();
    if (alarmRulesOverride) {
        JSON::saveAlarms(alarms, *alarmRulesOverride);
    } else {
        JSON::saveAlarms(alarms);
    }

    JsonObject heartbeat = doc[Keys::kHeartbeat].to<JsonObject>();
    JSON::saveHeartbeat(heartbeat);

    JsonObject udpPusher = doc[Keys::kUdpPusher].to<JsonObject>();
    JSON::saveUdpPusher(udpPusher);

    JsonObject airmouse = doc[Keys::kAirMouse].to<JsonObject>();
    JSON::saveAirMouse(airmouse);

    JsonObject imu = doc[Keys::kImu].to<JsonObject>();
    JSON::saveImu(imu);

    JsonObject matrix = doc[Keys::kMatrix].to<JsonObject>();
    JSON::saveMatrix(matrix);

    JsonObject macros = doc[Keys::kMacros].to<JsonObject>();
    JSON::saveMacros(macros);

    JsonObject keyboard = doc[Keys::kKeyboard].to<JsonObject>();
    JSON::saveKeyboard(keyboard);

    JsonObject logging = doc[Keys::kLogging].to<JsonObject>();
    JSON::saveLogging(logging);

    JsonObject power = doc[Keys::kPower].to<JsonObject>();
    JSON::savePower(power);

    JsonObject compensation = doc[Keys::kCompensation].to<JsonObject>();
    JSON::saveCompensation(compensation);

    JsonObject usbTerminal = doc[Keys::kUsbTerminal].to<JsonObject>();
    JSON::saveUsbTerminal(usbTerminal);

    JsonObject gpio = doc[Keys::kGpio].to<JsonObject>();
    JSON::saveGpio(gpio);
}

}  // namespace CONFIG::Serialization
