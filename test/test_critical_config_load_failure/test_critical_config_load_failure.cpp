#include <unity.h>

#include <ArduinoJson.h>

#include "../../src/core/config/ConfigManager.h"

// The native environment does not build production sources automatically.
// Compile the real section dispatcher and config-manager propagation path into
// this focused regression executable; persistence and unrelated section
// loaders remain deterministic test doubles below.
#include "../../src/core/config/serialization/ConfigLoaders.cpp"
#include "../../src/core/config/ConfigManager.cpp"
#include "../../src/system/rtc/RtcConfigLoader.cpp"

namespace {

enum class AlarmSectionShape : uint8_t {
    Missing,
    Object,
    WrongType,
};

enum class AlarmDependencySource : uint8_t {
    None,
    WifiCsi,
    Imu,
    Gpio,
};

enum class DependencySectionShape : uint8_t {
    Missing,
    Malformed,
    Canonical,
};

AlarmSectionShape gAlarmSectionShape = AlarmSectionShape::Object;
bool gDocumentReadSucceeds = true;
bool gAlarmLoadSucceeds = true;
uint32_t gAlarmLoadCalls = 0;
uint32_t gInitDefaultsCalls = 0;
uint32_t gMarkValidCalls = 0;
RTC::ConfigStore gRtcConfig{};
AlarmDependencySource gAlarmDependencySource = AlarmDependencySource::None;
DependencySectionShape gDependencySectionShape = DependencySectionShape::Missing;

}  // namespace

namespace LOG {

void Logging::log(esp_log_level_t, const char*, const char*, ...) {}
void Logging::setLevel(esp_log_level_t) {}

}  // namespace LOG

namespace CONFIG::JSON {

void loadNotification(JsonObject&) {}
void loadWifiSensing(JsonObject&) {}
void loadBle(JsonObject&) {}
void loadShelly(JsonObject&) {}
void loadHeartbeat(JsonObject&) {}
void loadUdpPusher(JsonObject&) {}
void loadAirMouse(JsonObject&) {}
void loadImu(JsonObject&) {}
void loadMatrix(JsonObject&) {}
void loadMatrixPsram(JsonObject&) {}
void loadMacros(JsonObjectConst) {}
void loadKeyboard(JsonObject&) {}
void loadLogging(JsonObject&) {}
void loadPower(JsonObject&) {}
void loadCompensation(JsonObject&) {}
void loadUsbTerminal(JsonObject&) {}
bool loadGpio(JsonObject&) { return true; }

bool loadAlarms(JsonObject&) {
    ++gAlarmLoadCalls;
    return gAlarmLoadSucceeds;
}

}  // namespace CONFIG::JSON

namespace CONFIG::Persistence {

void addDependencySection(SYSTEM::SpiRamJsonDocument& doc) {
    if (gAlarmDependencySource == AlarmDependencySource::None ||
        gDependencySectionShape == DependencySectionShape::Missing) {
        return;
    }

    if (gAlarmDependencySource == AlarmDependencySource::WifiCsi) {
        JsonObject wifi = doc[CONFIG::Keys::kWifiSensing].to<JsonObject>();
        if (gDependencySectionShape == DependencySectionShape::Malformed) {
            return;
        }
        wifi[CONFIG::Keys::kEnabled] = false;
        wifi[CONFIG::Keys::kSampleIntervalMs] = 200;
        wifi[CONFIG::Keys::kVarianceThreshold] = 4.0f;
        JsonObject csi = wifi[CONFIG::Keys::kCsiAlarm].to<JsonObject>();
        csi[CONFIG::Keys::kEnabled] = true;
        csi[CONFIG::Keys::kBands].to<JsonArray>();
        csi[CONFIG::Keys::kBaselineFrames] = 120;
        csi[CONFIG::Keys::kTopK] = 8;
        csi[CONFIG::Keys::kEnterThreshold] = 6.0f;
        csi[CONFIG::Keys::kClearThreshold] = 3.0f;
        csi[CONFIG::Keys::kHoldMs] = 500;
        csi[CONFIG::Keys::kClearHoldMs] = 1000;
        csi[CONFIG::Keys::kMinNoise] = 4.0f;
        csi[CONFIG::Keys::kMinEnergy] = 4.0f;
        csi[CONFIG::Keys::kNoisyThreshold] = 80.0f;
        csi[CONFIG::Keys::kAutoRecalibration] = true;
        csi[CONFIG::Keys::kSensitivity] = 1;
        return;
    }

    if (gAlarmDependencySource == AlarmDependencySource::Imu) {
        JsonObject imu = doc[CONFIG::Keys::kImu].to<JsonObject>();
        if (gDependencySectionShape == DependencySectionShape::Malformed) {
            return;
        }
        imu[CONFIG::Keys::kAlarmMonitorEnabled] = true;
        imu[CONFIG::Keys::kOrientationBaselineValid] = false;
        imu[CONFIG::Keys::kTiltThresholdDeg] = 30.0f;
        imu[CONFIG::Keys::kTiltHysteresisDeg] = 5.0f;
        imu[CONFIG::Keys::kTiltHoldMs] = 500;
        imu[CONFIG::Keys::kTiltClearHoldMs] = 1000;
        imu[CONFIG::Keys::kAccelDeltaThresholdG] = 0.35f;
        return;
    }

    JsonObject gpio = doc[CONFIG::Keys::kGpio].to<JsonObject>();
    if (gDependencySectionShape == DependencySectionShape::Malformed) {
        return;
    }
    JsonObject channel = gpio[CONFIG::Keys::kChannels]
                             .to<JsonArray>()
                             .add<JsonObject>();
    channel[CONFIG::Keys::kId] = "gpio1";
    channel[CONFIG::Keys::kPin] = 1;
    channel[CONFIG::Keys::kMode] = "input";
    channel[CONFIG::Keys::kPull] = "none";
    channel[CONFIG::Keys::kInverted] = false;
    channel[CONFIG::Keys::kDebounceMs] = 50;
    channel[CONFIG::Keys::kInitialOutput] = false;
}

bool readConfigDocument(FS&,
                        SYSTEM::SpiRamJsonDocument& doc,
                        const char*) {
    if (!gDocumentReadSucceeds) {
        return false;
    }

    doc.clear();
    switch (gAlarmSectionShape) {
        case AlarmSectionShape::Missing:
            break;
        case AlarmSectionShape::Object:
            {
                JsonObject alarms = doc[CONFIG::Keys::kAlarms].to<JsonObject>();
                if (gAlarmDependencySource != AlarmDependencySource::None) {
                    JsonObject rule =
                        alarms[CONFIG::Keys::kRules].to<JsonArray>().add<JsonObject>();
                    rule[CONFIG::Keys::kEnabled] = true;
                    switch (gAlarmDependencySource) {
                        case AlarmDependencySource::WifiCsi:
                            rule[CONFIG::Keys::kSource] = "wifi_csi_motion";
                            break;
                        case AlarmDependencySource::Imu:
                            rule[CONFIG::Keys::kSource] = "imu_tamper";
                            break;
                        case AlarmDependencySource::Gpio:
                            rule[CONFIG::Keys::kSource] = "gpio_digital";
                            rule[CONFIG::Keys::kGpioId] = "gpio1";
                            break;
                        case AlarmDependencySource::None:
                            break;
                    }
                }
            }
            break;
        case AlarmSectionShape::WrongType:
            doc[CONFIG::Keys::kAlarms] = "invalid";
            break;
    }
    addDependencySection(doc);
    return true;
}

bool writeConfigDocumentAtomically(FS&, SYSTEM::SpiRamJsonDocument&) {
    return true;
}

void deleteConfigFile(FS&) {}

}  // namespace CONFIG::Persistence

namespace CONFIG::Serialization {

void buildConfigDocument(SYSTEM::SpiRamJsonDocument&,
                         const ALARMS::AlarmRulesSnapshot*) {}

}  // namespace CONFIG::Serialization

namespace RTC {

const ConfigStore& getConfig() {
    return gRtcConfig;
}

bool isValid() {
    return false;
}

void initDefaults() {
    ++gInitDefaultsCalls;
    gRtcConfig = ConfigStore{};
}

void markValid() {
    ++gMarkValidCalls;
}

void validateRuntimeData() {}

bool consumeMaintenanceWakeFlag() {
    return false;
}

void logStatus() {}

}  // namespace RTC

void setUp() {
    fsStubReset();
    fsStubSetFileExists(CONFIG::kConfigFile, true);
    gAlarmSectionShape = AlarmSectionShape::Object;
    gDocumentReadSucceeds = true;
    gAlarmLoadSucceeds = true;
    gAlarmLoadCalls = 0;
    gInitDefaultsCalls = 0;
    gMarkValidCalls = 0;
    gRtcConfig = RTC::ConfigStore{};
    gAlarmDependencySource = AlarmDependencySource::None;
    gDependencySectionShape = DependencySectionShape::Missing;
}

void tearDown() {}

void test_alarm_loader_failure_propagates_as_critical_config_failure() {
    FS fs;
    gAlarmLoadSucceeds = false;
    CONFIG::LoadFailure failure = CONFIG::LoadFailure::None;

    TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::LoadFailure::CriticalSection),
        static_cast<uint8_t>(failure));
    TEST_ASSERT_EQUAL_UINT32(1, gAlarmLoadCalls);
}

void test_wrong_alarm_section_type_fails_before_alarm_loader() {
    FS fs;
    gAlarmSectionShape = AlarmSectionShape::WrongType;
    CONFIG::LoadFailure failure = CONFIG::LoadFailure::None;

    TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::LoadFailure::CriticalSection),
        static_cast<uint8_t>(failure));
    TEST_ASSERT_EQUAL_UINT32(0, gAlarmLoadCalls);
}

void test_missing_alarm_section_is_a_critical_config_failure() {
    FS fs;
    gAlarmSectionShape = AlarmSectionShape::Missing;
    CONFIG::LoadFailure failure = CONFIG::LoadFailure::None;

    TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::LoadFailure::CriticalSection),
        static_cast<uint8_t>(failure));
    TEST_ASSERT_EQUAL_UINT32(0, gAlarmLoadCalls);
}

void test_document_and_missing_file_failures_remain_distinguishable() {
    FS fs;
    CONFIG::LoadFailure failure = CONFIG::LoadFailure::None;

    gDocumentReadSucceeds = false;
    TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::LoadFailure::InvalidDocument),
        static_cast<uint8_t>(failure));

    fsStubSetFileExists(CONFIG::kConfigFile, false);
    failure = CONFIG::LoadFailure::None;
    TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::LoadFailure::NotFound),
        static_cast<uint8_t>(failure));
}

void test_enabled_alarm_sources_require_their_persisted_dependency_sections() {
    FS fs;
    const AlarmDependencySource sources[] = {
        AlarmDependencySource::WifiCsi,
        AlarmDependencySource::Imu,
        AlarmDependencySource::Gpio,
    };

    for (AlarmDependencySource source : sources) {
        gAlarmDependencySource = source;

        gDependencySectionShape = DependencySectionShape::Missing;
        CONFIG::LoadFailure failure = CONFIG::LoadFailure::None;
        TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(CONFIG::LoadFailure::CriticalSection),
            static_cast<uint8_t>(failure));

        gDependencySectionShape = DependencySectionShape::Malformed;
        failure = CONFIG::LoadFailure::None;
        TEST_ASSERT_FALSE(CONFIG::load(fs, &failure));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(CONFIG::LoadFailure::CriticalSection),
            static_cast<uint8_t>(failure));

        gDependencySectionShape = DependencySectionShape::Canonical;
        failure = CONFIG::LoadFailure::CriticalSection;
        TEST_ASSERT_TRUE(CONFIG::load(fs, &failure));
        TEST_ASSERT_EQUAL_UINT8(
            static_cast<uint8_t>(CONFIG::LoadFailure::None),
            static_cast<uint8_t>(failure));
    }
}

void test_psram_hydration_propagates_critical_alarm_failure() {
    FS fs;
    gAlarmLoadSucceeds = false;
    CONFIG::LoadFailure failure = CONFIG::LoadFailure::None;

    TEST_ASSERT_FALSE(CONFIG::loadPsramOnly(fs, &failure));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CONFIG::LoadFailure::CriticalSection),
        static_cast<uint8_t>(failure));
    TEST_ASSERT_EQUAL_UINT32(1, gAlarmLoadCalls);
}

void test_critical_alarm_failure_prevents_rtc_boot_completion() {
    FS fs;
    gAlarmLoadSucceeds = false;

    TEST_ASSERT_FALSE(RTC::reloadAllFromFS(fs));
    TEST_ASSERT_EQUAL_UINT32(1, gInitDefaultsCalls);
    TEST_ASSERT_EQUAL_UINT32(0, gMarkValidCalls);
}

void test_malformed_existing_document_prevents_rtc_boot_completion() {
    FS fs;
    gDocumentReadSucceeds = false;

    TEST_ASSERT_FALSE(RTC::reloadAllFromFS(fs));
    TEST_ASSERT_EQUAL_UINT32(1, gInitDefaultsCalls);
    TEST_ASSERT_EQUAL_UINT32(0, gMarkValidCalls);
}

void test_noncritical_missing_file_keeps_factory_fallback_policy() {
    FS fs;
    fsStubSetFileExists(CONFIG::kConfigFile, false);

    TEST_ASSERT_TRUE(RTC::reloadAllFromFS(fs));
    TEST_ASSERT_EQUAL_UINT32(1, gInitDefaultsCalls);
    TEST_ASSERT_EQUAL_UINT32(1, gMarkValidCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_alarm_loader_failure_propagates_as_critical_config_failure);
    RUN_TEST(test_wrong_alarm_section_type_fails_before_alarm_loader);
    RUN_TEST(test_missing_alarm_section_is_a_critical_config_failure);
    RUN_TEST(test_document_and_missing_file_failures_remain_distinguishable);
    RUN_TEST(test_enabled_alarm_sources_require_their_persisted_dependency_sections);
    RUN_TEST(test_psram_hydration_propagates_critical_alarm_failure);
    RUN_TEST(test_critical_alarm_failure_prevents_rtc_boot_completion);
    RUN_TEST(test_malformed_existing_document_prevents_rtc_boot_completion);
    RUN_TEST(test_noncritical_missing_file_keeps_factory_fallback_policy);
    return UNITY_END();
}
