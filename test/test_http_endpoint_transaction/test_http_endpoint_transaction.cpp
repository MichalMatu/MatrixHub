#ifdef UNIT_TEST

#include <unity.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../test/stubs/FS.h"
#include "../../test/stubs/PsychicRequest.h"
#include "../../test/stubs/PsychicJson.h"
#include "../../test/stubs/PsychicHttpServer.h"
#include "../../test/stubs/esp_log.h"
#include "../../test/stubs/freertos/semphr.h"

#define SVK_TAG "Test"

#include "../../lib/framework/core/HttpEndpoint.h"
#include "../../src/notifications/settings/NotificationConfigStore.h"
#include "../../src/alarms/AlarmSettingsPayload.h"
#include "../../src/notifications/settings/NotificationSettingsService.h"
#include "../../src/alarms/AlarmSettingsService.h"
#include "../../src/macros/MacroSettingsService.h"
#include "../../src/keyboard/KeyboardSettingsService.h"
#include "../../src/matrix/MatrixCustomIconStore.h"
#include "../../src/matrix/MatrixSettingsService.h"
#include "../../src/compensation/CompensationSettingsService.h"
#include "../../src/system/health/heartbeat/HeartbeatSettingsService.h"
#include "../../src/system/power/PowerSettingsService.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/system/rtc/RtcStatefulService.h"
#include "../../src/udp/UdpSettingsService.h"
#include "../../src/airmouse/AirMouseSettingsService.h"
#include "../../src/usb_terminal/UsbTerminalSettingsService.h"

#include "../../src/airmouse/AirMouseSettingsService.cpp"
#include "../../src/alarms/AlarmSettingsPayload.cpp"
#include "../../src/alarms/AlarmSettingsService.cpp"
#include "../../src/compensation/CompensationSettingsService.cpp"
#include "../../src/macros/MacroSettingsService.cpp"
#include "../../src/keyboard/KeyboardSettingsService.cpp"
#include "../../src/matrix/MatrixCustomIconStore.cpp"
#include "../../src/matrix/MatrixSettingsService.cpp"
#include "../../src/notifications/settings/NotificationConfigStore.cpp"
#include "../../src/notifications/settings/NotificationSettingsAdapter.cpp"
#include "../../src/notifications/settings/NotificationSettingsService.cpp"
#include "../../src/system/health/heartbeat/HeartbeatSettingsService.cpp"
#include "../../src/system/power/PowerSettingsService.cpp"
#include "../../src/udp/UdpSettingsService.cpp"
#include "../../src/usb_terminal/UsbTerminalSettingsService.cpp"

update_handler_id_t StateUpdateHandlerInfo::currentUpdatedHandlerId = 0;
hook_handler_id_t StateHookHandlerInfo::currentHookHandlerId = 0;

namespace LOG {
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
}  // namespace LOG

namespace COMPENSATION {
void CompensationService::applySettings() {}
}  // namespace COMPENSATION

namespace MACROS {
void MacroService::applySettings() {}
}  // namespace MACROS

namespace ALARMS {
bool AlarmService::isAlarmLatched() const { return false; }
void AlarmService::reapplyLatchedState() {}
}  // namespace ALARMS

namespace MATRIX_MANAGER {
void MatrixManagerService::setLayer(Layer layer, const LayerContent& content) {
    (void)layer;
    (void)content;
}
void MatrixManagerService::clearLayer(Layer layer) {
    (void)layer;
}
void MatrixManagerService::invalidateCache() {}
}  // namespace MATRIX_MANAGER

namespace MATRIX {
void MatrixMenuService::invalidateCache() {}
}  // namespace MATRIX

namespace CONFIG {
bool shouldSaveSucceed = true;
bool shouldSaveAlarmRulesSucceed = true;
int failSaveAlarmRulesOnCall = -1;
RTC::AlarmRulesData lastSavedAlarmRules{};
int saveAlarmRulesCallCount = 0;

bool save(FS& fs) {
    (void)fs;
    return shouldSaveSucceed;
}

bool saveWithAlarmRules(FS& fs, const RTC::AlarmRulesData& alarmRules) {
    (void)fs;
    lastSavedAlarmRules = alarmRules;
    ++saveAlarmRulesCallCount;
    if (saveAlarmRulesCallCount == failSaveAlarmRulesOnCall) {
        return false;
    }
    return shouldSaveAlarmRulesSucceed;
}
}  // namespace CONFIG

namespace CONFIG {
namespace JSON {

void deserializeUdpPusher(JsonObject& obj, RTC::UdpPusherData& data) {
    if (obj[CONFIG::Keys::kEnabled].is<bool>()) {
        data.enabled = obj[CONFIG::Keys::kEnabled].as<bool>();
    }
    if (const char* host = obj[CONFIG::Keys::kHost] | (const char*)nullptr) {
        strncpy(data.host, host, sizeof(data.host));
        data.host[sizeof(data.host) - 1] = '\0';
    }
    if (obj[CONFIG::Keys::kPort].is<uint16_t>()) {
        data.port = obj[CONFIG::Keys::kPort].as<uint16_t>();
    }
    if (obj[CONFIG::Keys::kIntervalMs].is<uint32_t>()) {
        data.intervalMs = obj[CONFIG::Keys::kIntervalMs].as<uint32_t>();
    }
    if (const char* format = obj[CONFIG::Keys::kFormat] | (const char*)nullptr) {
        if (strcmp(format, "json") == 0) {
            data.format = RTC::UdpFormat::Json;
        } else if (strcmp(format, "csv") == 0) {
            data.format = RTC::UdpFormat::Csv;
        } else {
            data.format = RTC::UdpFormat::LineProtocol;
        }
    }
}

void deserializeCompensation(JsonObject& obj, RTC::CompensationData& data) {
    if (obj[CONFIG::Keys::kEnabled].is<bool>()) {
        data.enabled = obj[CONFIG::Keys::kEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kBaseTempOffset].is<float>()) {
        data.baseTempOffset = obj[CONFIG::Keys::kBaseTempOffset].as<float>();
    }
    if (obj[CONFIG::Keys::kReferenceCpuTemp].is<float>()) {
        data.referenceCpuTemp = obj[CONFIG::Keys::kReferenceCpuTemp].as<float>();
    }
    if (obj[CONFIG::Keys::kTempOffsetPerCpuDegree].is<float>()) {
        data.tempOffsetPerCpuDegree = obj[CONFIG::Keys::kTempOffsetPerCpuDegree].as<float>();
    }
    if (obj[CONFIG::Keys::kMinTempOffset].is<float>()) {
        data.minTempOffset = obj[CONFIG::Keys::kMinTempOffset].as<float>();
    }
    if (obj[CONFIG::Keys::kMaxTempOffset].is<float>()) {
        data.maxTempOffset = obj[CONFIG::Keys::kMaxTempOffset].as<float>();
    }
}

void deserializeHeartbeat(JsonObject& obj, RTC::HeartbeatData& data) {
    if (obj[CONFIG::Keys::kIntervalMs].is<uint32_t>()) {
        data.intervalMs = obj[CONFIG::Keys::kIntervalMs].as<uint32_t>();
    }
}

void deserializeMacros(JsonObjectConst obj, RTC::MacroData& data) {
    if (obj[CONFIG::Keys::kEnabled].is<bool>()) {
        data.enabled = obj[CONFIG::Keys::kEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kBootDelay].is<uint32_t>()) {
        data.bootDelay = obj[CONFIG::Keys::kBootDelay].as<uint32_t>();
    }
    if (const char* script = obj[CONFIG::Keys::kBootScript] | (const char*)nullptr) {
        strncpy(data.bootScript, script, sizeof(data.bootScript));
        data.bootScript[sizeof(data.bootScript) - 1] = '\0';
    }
}

void deserializeUsbTerminal(JsonObject& obj, RTC::UsbTerminalData& data) {
    if (obj[CONFIG::Keys::kEnabled].is<bool>()) {
        data.enabled = obj[CONFIG::Keys::kEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kIdleTimeoutMs].is<uint32_t>()) {
        data.idleTimeoutMs = obj[CONFIG::Keys::kIdleTimeoutMs].as<uint32_t>();
    }
    if (const char* port = obj[CONFIG::Keys::kTargetPort] | (const char*)nullptr) {
        strncpy(data.targetPort, port, sizeof(data.targetPort));
        data.targetPort[sizeof(data.targetPort) - 1] = '\0';
    }
}

void deserializeKeyboard(JsonObject& obj, RTC::KeyboardData& data) {
    if (obj[CONFIG::Keys::kEnabled].is<bool>()) {
        data.enabled = obj[CONFIG::Keys::kEnabled].as<bool>();
    }
}

void deserializeAirMouse(JsonObject& obj, RTC::AirMouseData& data) {
    if (obj[CONFIG::Keys::kMovementEnabled].is<bool>()) {
        data.movementEnabled = obj[CONFIG::Keys::kMovementEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kClickEnabled].is<bool>()) {
        data.clickEnabled = obj[CONFIG::Keys::kClickEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kSensitivityX].is<float>()) {
        data.sensitivityX = obj[CONFIG::Keys::kSensitivityX].as<float>();
    }
}

void deserializePower(JsonObject& obj, RTC::PowerData& data) {
    if (obj[CONFIG::Keys::kSleepEnabled].is<bool>()) {
        data.sleepEnabled = obj[CONFIG::Keys::kSleepEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kInactivityTimeoutMs].is<uint32_t>()) {
        data.inactivityTimeoutMs = obj[CONFIG::Keys::kInactivityTimeoutMs].as<uint32_t>();
    }
    if (obj[CONFIG::Keys::kGraceAfterBootMs].is<uint32_t>()) {
        data.graceAfterBootMs = obj[CONFIG::Keys::kGraceAfterBootMs].as<uint32_t>();
    }
    if (obj[CONFIG::Keys::kWakeTimerEnabled].is<bool>()) {
        data.wakeTimerEnabled = obj[CONFIG::Keys::kWakeTimerEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kWakeButtonEnabled].is<bool>()) {
        data.wakeButtonEnabled = obj[CONFIG::Keys::kWakeButtonEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kWakeTouchEnabled].is<bool>()) {
        data.wakeTouchEnabled = obj[CONFIG::Keys::kWakeTouchEnabled].as<bool>();
    }
}

void deserializeMatrix(JsonObject& obj, RTC::MatrixData& data) {
    if (obj[CONFIG::Keys::kBrightness].is<uint8_t>()) {
        data.brightness = obj[CONFIG::Keys::kBrightness].as<uint8_t>();
    }
    if (obj[CONFIG::Keys::kAlarmMode].is<uint8_t>()) {
        data.alarmMode = static_cast<RTC::MatrixAlarmMode>(obj[CONFIG::Keys::kAlarmMode].as<uint8_t>());
    }
    if (obj[CONFIG::Keys::kRotation].is<uint8_t>()) {
        data.rotation = obj[CONFIG::Keys::kRotation].as<uint8_t>();
    }
    if (obj[CONFIG::Keys::kAutoRotate].is<bool>()) {
        data.autoRotate = obj[CONFIG::Keys::kAutoRotate].as<bool>();
    }
    if (obj[CONFIG::Keys::kEffectEnabled].is<bool>()) {
        data.effectEnabled = obj[CONFIG::Keys::kEffectEnabled].as<bool>();
    }
    if (obj[CONFIG::Keys::kEffectMode].is<uint8_t>()) {
        data.effectMode = obj[CONFIG::Keys::kEffectMode].as<uint8_t>();
    }
    if (obj[CONFIG::Keys::kEffectSpeed].is<uint16_t>()) {
        data.effectSpeed = obj[CONFIG::Keys::kEffectSpeed].as<uint16_t>();
    }
    if (obj[CONFIG::Keys::kEffectColor].is<uint32_t>()) {
        data.effectColor = obj[CONFIG::Keys::kEffectColor].as<uint32_t>();
    }
    if (obj[CONFIG::Keys::kEffectColor2].is<uint32_t>()) {
        data.effectColor2 = obj[CONFIG::Keys::kEffectColor2].as<uint32_t>();
    }
    if (obj[CONFIG::Keys::kEffectColor3].is<uint32_t>()) {
        data.effectColor3 = obj[CONFIG::Keys::kEffectColor3].as<uint32_t>();
    }
    if (obj[CONFIG::Keys::kMenuTextColor].is<uint32_t>()) {
        data.menu.textColor = obj[CONFIG::Keys::kMenuTextColor].as<uint32_t>();
    }
    if (obj[CONFIG::Keys::kMenuScrollSpeed].is<uint16_t>()) {
        data.menu.scrollSpeed = obj[CONFIG::Keys::kMenuScrollSpeed].as<uint16_t>();
    }
    if (obj[CONFIG::Keys::kMenuEnabled].is<bool>()) {
        data.menu.enabled = obj[CONFIG::Keys::kMenuEnabled].as<bool>();
    }
}

void serializeMatrix(const RTC::MatrixData& data, JsonObject& obj) {
    obj[CONFIG::Keys::kBrightness] = data.brightness;
    obj[CONFIG::Keys::kAlarmMode] = static_cast<uint8_t>(data.alarmMode);
    obj[CONFIG::Keys::kRotation] = data.rotation;
    obj[CONFIG::Keys::kAutoRotate] = data.autoRotate;
    obj[CONFIG::Keys::kEffectEnabled] = data.effectEnabled;
    obj[CONFIG::Keys::kEffectMode] = data.effectMode;
    obj[CONFIG::Keys::kEffectSpeed] = data.effectSpeed;
    obj[CONFIG::Keys::kEffectColor] = data.effectColor;
    obj[CONFIG::Keys::kEffectColor2] = data.effectColor2;
    obj[CONFIG::Keys::kEffectColor3] = data.effectColor3;
    obj[CONFIG::Keys::kMenuTextColor] = data.menu.textColor;
    obj[CONFIG::Keys::kMenuScrollSpeed] = data.menu.scrollSpeed;
    obj[CONFIG::Keys::kMenuEnabled] = data.menu.enabled;
}

void saveAlarms(JsonObject& obj, const RTC::AlarmRulesData& rules) {
    JsonArray rulesArray = obj[CONFIG::Keys::kRules].to<JsonArray>();
    for (uint8_t i = 0; i < rules.ruleCount; i++) {
        JsonObject rule = rulesArray.add<JsonObject>();
        rule[CONFIG::Keys::kId] = rules.rules[i].id;
        rule[CONFIG::Keys::kName] = rules.rules[i].name;
        rule[CONFIG::Keys::kEnabled] = rules.rules[i].enabled;
    }
}

bool deserializeAlarmRules(
    JsonArray& rules,
    RTC::AlarmRulesData& parsed,
    AlarmRulesParseError* error) {
    memset(&parsed, 0, sizeof(parsed));
    if (error) {
        *error = AlarmRulesParseError::None;
    }

    for (JsonObject ruleObject : rules) {
        if (parsed.ruleCount >= RTC::kMaxAlarmRules) {
            break;
        }

        const char* id = ruleObject[CONFIG::Keys::kId] | static_cast<const char*>(nullptr);
        const char* name = ruleObject[CONFIG::Keys::kName] | static_cast<const char*>(nullptr);
        if (!id || id[0] == '\0' || !name || name[0] == '\0') {
            if (error) {
                *error = AlarmRulesParseError::InvalidRuleData;
            }
            return false;
        }

        for (uint8_t i = 0; i < parsed.ruleCount; i++) {
            if (strcmp(parsed.rules[i].id, id) == 0) {
                if (error) {
                    *error = AlarmRulesParseError::DuplicateRuleId;
                }
                return false;
            }
        }

        ALARMS::AlarmRule rule;
        strncpy(rule.id, id, sizeof(rule.id));
        rule.id[sizeof(rule.id) - 1] = '\0';
        strncpy(rule.name, name, sizeof(rule.name));
        rule.name[sizeof(rule.name) - 1] = '\0';
        rule.enabled = ruleObject[CONFIG::Keys::kEnabled] | false;
        parsed.rules[parsed.ruleCount++] = rule;
    }

    return true;
}

}  // namespace JSON
}  // namespace CONFIG

namespace RTC {

ConfigStore mockStore{};
ConfigStore* store = &mockStore;
RtcSensorState sensorState{};
RtcHeapHistory heapHistory{};
RtcRuntimeStats runtimeStats{};
RtcNetworkState networkState{};
bool lockEnabled = true;
int failLockAfterCall = -1;
int lockCallCount = 0;

const ConfigStore& getConfig() { return mockStore; }
ConfigStore getConfigSafeCopy() { return mockStore; }
ConfigStore& getMutableConfig() { return mockStore; }
SemaphoreHandle_t getLock() {
    static SemaphoreHandle_t lock = xSemaphoreCreateMutex();
    ++lockCallCount;
    if (!lockEnabled) {
        return nullptr;
    }
    if (failLockAfterCall >= 0 && lockCallCount > failLockAfterCall) {
        return nullptr;
    }
    return lock;
}
void withConfig(const std::function<void(const ConfigStore&)>& reader) { reader(mockStore); }
void createLock() {}
void init() {}
bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    updater(mockStore);
    return true;
}
void updateConfigLocked(const std::function<void(ConfigStore&)>& updater) {
    updater(mockStore);
}
void markValid() {}

}  // namespace RTC

namespace SYSTEM {
namespace HEARTBEAT_CONFIG {

RTC::HeartbeatData heartbeatState{};

RTC::HeartbeatData copy() {
    return heartbeatState;
}

void withConfig(const std::function<void(const RTC::HeartbeatData&)>& reader) {
    reader(heartbeatState);
}

bool update(const std::function<void(RTC::HeartbeatData&)>& updater) {
    updater(heartbeatState);
    return true;
}

}  // namespace HEARTBEAT_CONFIG
}  // namespace SYSTEM

namespace ALARMS {
namespace RULES_CONFIG {

RTC::AlarmRulesData alarmRulesState{};

bool copyTo(RTC::AlarmRulesData& out) {
    out = alarmRulesState;
    return true;
}

void withRules(const std::function<void(const RTC::AlarmRulesData&)>& reader) {
    reader(alarmRulesState);
}

bool update(const std::function<void(RTC::AlarmRulesData&)>& updater) {
    updater(alarmRulesState);
    return true;
}

}  // namespace RULES_CONFIG
}  // namespace ALARMS

namespace {

struct TestState {
    int value = 1;
    bool persisted = false;

    static void read(TestState& state, JsonObject& root) {
        root["value"] = state.value;
        root["persisted"] = state.persisted;
    }

    static StateUpdateResult update(JsonObject& root, TestState& state, std::string_view originId) {
        (void)originId;
        const int nextValue = root["value"] | state.value;
        if (nextValue == state.value) {
            return StateUpdateResult::UNCHANGED;
        }

        state.value = nextValue;
        state.persisted = false;
        return StateUpdateResult::CHANGED;
    }
};

class TestStateService : public StatefulService<TestState> {
public:
    explicit TestStateService(bool failFirstHandler = false)
        : _failFirstHandler(failFirstHandler) {
        addUpdateHandler([this](std::string_view originId) {
            (void)originId;
            firstHandlerRan = true;
            if (_failFirstHandler) {
                return StateHandlerResult::failure("config/save_failed");
            }
            _state.persisted = true;
            return StateHandlerResult::success();
        }, false);

        addUpdateHandler([this](std::string_view originId) {
            (void)originId;
            secondHandlerRan = true;
            return StateHandlerResult::success();
        }, false);
    }

    bool firstHandlerRan = false;
    bool secondHandlerRan = false;

private:
    bool _failFirstHandler = false;
};

class TestRtcService : public RtcStatefulService<RTC::BleData> {
public:
    TestRtcService()
        : RtcStatefulService<RTC::BleData>(&RTC::ConfigStore::ble) {}

    bool cachedEnabled() const {
        return _state.enabled;
    }
};

class ReadFailingStateService : public StatefulService<TestState> {
public:
    StateHandlerResult read(std::function<void(TestState&)> stateReader) override {
        (void)stateReader;
        return StateHandlerResult::failure("internal/update_failed", 503);
    }

    StateHandlerResult read(JsonObject& jsonObject, JsonStateReader<TestState> stateReader) override {
        (void)jsonObject;
        (void)stateReader;
        return StateHandlerResult::failure("internal/update_failed", 503);
    }
};

class BlockingHandlerStateService : public StatefulService<TestState> {
public:
    BlockingHandlerStateService() {
        addUpdateHandler([this](std::string_view originId) {
            (void)originId;
            {
                std::lock_guard<std::mutex> lock(_gateMutex);
                _handlerEntered = true;
            }
            _gateCv.notify_all();

            std::unique_lock<std::mutex> lock(_gateMutex);
            _gateCv.wait(lock, [this]() { return _releaseHandler; });
            _state.persisted = true;
            return StateHandlerResult::success();
        }, false);
    }

    void waitForHandler() {
        std::unique_lock<std::mutex> lock(_gateMutex);
        _gateCv.wait(lock, [this]() { return _handlerEntered; });
    }

    void releaseHandler() {
        {
            std::lock_guard<std::mutex> lock(_gateMutex);
            _releaseHandler = true;
        }
        _gateCv.notify_all();
    }

private:
    std::mutex _gateMutex;
    std::condition_variable _gateCv;
    bool _handlerEntered = false;
    bool _releaseHandler = false;
};

std::string jsonBody(JsonDocument& doc) {
    std::vector<char> buffer(measureJson(doc) + 1, '\0');
    serializeJson(doc, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

void copyUdpHost(char (&target)[64], const char* source) {
    strncpy(target, source, sizeof(target));
    target[sizeof(target) - 1] = '\0';
}

void resetRtcStore() {
    RTC::mockStore = RTC::ConfigStore{};
    RTC::lockEnabled = true;
    RTC::failLockAfterCall = -1;
    RTC::lockCallCount = 0;
    CONFIG::shouldSaveSucceed = true;
    CONFIG::shouldSaveAlarmRulesSucceed = true;
    CONFIG::failSaveAlarmRulesOnCall = -1;
    CONFIG::lastSavedAlarmRules = RTC::AlarmRulesData{};
    CONFIG::saveAlarmRulesCallCount = 0;
    SYSTEM::HEARTBEAT_CONFIG::heartbeatState = RTC::HeartbeatData{};
    ALARMS::RULES_CONFIG::alarmRulesState = RTC::AlarmRulesData{};
    MATRIX::clearAllCustomIcons();
}

void resetNotificationStore() {
    TEST_ASSERT_TRUE(NOTIFICATIONS::CONFIG_STORE::update([](RTC::NotificationData& state) {
        state = RTC::NotificationData{};
    }));
}

}  // namespace

void setUp(void) {
    resetRtcStore();
    resetNotificationStore();
}

void tearDown(void) {}

void test_http_endpoint_changed_returns_200_after_handlers_finish() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN);
    endpoint.begin();

    TEST_ASSERT_TRUE(server.hasRequestHandler("/api/test", HTTP_POST));

    PsychicRequest request;
    JsonDocument doc;
    doc["value"] = 7;
    request.setBody(jsonBody(doc));

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_TRUE(service.firstHandlerRan);
    TEST_ASSERT_TRUE(service.secondHandlerRan);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"value\":7") != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"persisted\":true") != std::string::npos);
}

void test_http_endpoint_failed_handler_returns_500_and_rolls_back_state() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(true);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN);
    endpoint.begin();

    PsychicRequest request;
    JsonDocument doc;
    doc["value"] = 9;
    request.setBody(jsonBody(doc));

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(500, request.lastStatusCode);
    TEST_ASSERT_TRUE(service.firstHandlerRan);
    TEST_ASSERT_FALSE(service.secondHandlerRan);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"error\":\"config/save_failed\"") != std::string::npos);

    TestState snapshot{};
    service.read([&](TestState& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(1, snapshot.value);
    TEST_ASSERT_FALSE(snapshot.persisted);
}

void test_http_endpoint_unchanged_skips_handlers_and_returns_current_state() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN);
    endpoint.begin();

    PsychicRequest request;
    JsonDocument doc;
    doc["value"] = 1;
    request.setBody(jsonBody(doc));

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(200, request.lastStatusCode);
    TEST_ASSERT_FALSE(service.firstHandlerRan);
    TEST_ASSERT_FALSE(service.secondHandlerRan);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"value\":1") != std::string::npos);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"persisted\":false") != std::string::npos);
}

void test_http_endpoint_validator_failure_returns_error_without_updating_state() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN,
        [](PsychicRequest* request, JsonObject& root) {
            (void)request;
            if (root["blocked"] | false) {
                return StateHandlerResult::failure("input/blocked", 409);
            }
            return StateHandlerResult::success();
        });
    endpoint.begin();

    PsychicRequest request;
    JsonDocument doc;
    doc["value"] = 8;
    doc["blocked"] = true;
    request.setBody(jsonBody(doc));

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(409, request.lastStatusCode);
    TEST_ASSERT_FALSE(service.firstHandlerRan);
    TEST_ASSERT_FALSE(service.secondHandlerRan);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"error\":\"input/blocked\"") != std::string::npos);

    TestState snapshot{};
    service.read([&](TestState& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(1, snapshot.value);
    TEST_ASSERT_FALSE(snapshot.persisted);
}

void test_http_endpoint_post_notifies_activity_callback() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    int activityCalls = 0;
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN,
        nullptr,
        [&activityCalls]() {
            ++activityCalls;
        });
    endpoint.begin();

    PsychicRequest request;
    JsonDocument doc;
    doc["value"] = 4;
    request.setBody(jsonBody(doc));

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(1, activityCalls);
}

void test_http_endpoint_invalid_json_returns_structured_error() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN);
    endpoint.begin();

    PsychicRequest request;
    request.setBody("{");

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"error\":\"input/invalid_json\"") != std::string::npos);
}

void test_http_endpoint_payload_too_large_returns_structured_error() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN);
    endpoint.begin();

    PsychicRequest request;
    request.setBody(std::string(8200, 'x'));

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(413, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"error\":\"input/payload_too_large\"") != std::string::npos);
}

void test_http_endpoint_validator_rejects_unexpected_fields() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    TestStateService service(false);
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN,
        [](PsychicRequest* request, JsonObject& root) {
            (void)request;
            for (JsonPair pair : root) {
                if (strcmp(pair.key().c_str(), "value") != 0) {
                    return StateHandlerResult::failure("input/unexpected_field", 400);
                }
            }
            return StateHandlerResult::success();
        });
    endpoint.begin();

    PsychicRequest request;
    request.setBody("{\"value\":4,\"extra\":true}");

    const esp_err_t err = server.invoke("/api/test", HTTP_POST, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"error\":\"input/unexpected_field\"") != std::string::npos);
    int storedValue = 0;
    service.read([&storedValue](TestState& state) {
        storedValue = state.value;
    });
    TEST_ASSERT_EQUAL(1, storedValue);
}

void test_http_endpoint_get_returns_error_when_state_read_fails() {
    PsychicHttpServer server;
    SecurityManager securityManager;
    ReadFailingStateService service;
    HttpEndpoint<TestState> endpoint(
        TestState::read,
        TestState::update,
        &service,
        &server,
        "/api/test",
        &securityManager,
        AuthenticationPredicates::IS_ADMIN);
    endpoint.begin();

    PsychicRequest request;

    const esp_err_t err = server.invoke("/api/test", HTTP_GET, &request);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(503, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find("\"error\":\"internal/update_failed\"") != std::string::npos);
}

void test_rtc_stateful_service_rolls_back_store_on_handler_failure() {
    RTC::mockStore.ble.enabled = false;
    TestRtcService service;
    service.addUpdateHandler([](std::string_view originId) {
        (void)originId;
        return StateHandlerResult::failure("config/save_failed");
    }, false);

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::BleData& state) {
            state.enabled = true;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_FALSE(RTC::getConfig().ble.enabled);

    RTC::BleData snapshot{};
    service.read([&](RTC::BleData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.enabled);
}

void test_rtc_stateful_service_failed_commit_restores_cached_state() {
    RTC::mockStore.ble.enabled = false;
    TestRtcService service;
    RTC::lockCallCount = 0;
    RTC::failLockAfterCall = 1;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::BleData& state) {
            state.enabled = true;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_FALSE(RTC::getConfig().ble.enabled);
    TEST_ASSERT_FALSE(service.cachedEnabled());

    RTC::failLockAfterCall = -1;
}

void test_stateful_service_update_without_propagation_rolls_back_on_error() {
    TestStateService service(false);

    const StateUpdateResult result = service.updateWithoutPropagation(
        [](TestState& state) {
            state.value = 11;
            return StateUpdateResult::ERROR;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result);

    TestState snapshot{};
    service.read([&](TestState& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(1, snapshot.value);
    TEST_ASSERT_FALSE(snapshot.persisted);
}

void test_rtc_stateful_service_update_without_propagation_restores_state_on_commit_failure() {
    RTC::mockStore.ble.enabled = false;
    TestRtcService service;
    RTC::lockCallCount = 0;
    RTC::failLockAfterCall = 1;

    const StateUpdateResult result = service.updateWithoutPropagation(
        [](RTC::BleData& state) {
            state.enabled = true;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result);
    TEST_ASSERT_FALSE(RTC::getConfig().ble.enabled);
    TEST_ASSERT_FALSE(service.cachedEnabled());

    RTC::failLockAfterCall = -1;
}

void test_stateful_service_blocks_second_update_until_handlers_finish() {
    BlockingHandlerStateService service;

    StateTransactionResult firstResult{};
    StateTransactionResult secondResult{};
    std::atomic<bool> secondStarted{false};
    std::atomic<bool> secondFinished{false};

    std::thread first([&]() {
        firstResult = service.updateAndPropagate(
            [](TestState& state) {
                state.value = 7;
                return StateUpdateResult::CHANGED;
            },
            "first");
    });

    service.waitForHandler();

    std::thread second([&]() {
        secondStarted.store(true);
        secondResult = service.updateAndPropagate(
            [](TestState& state) {
                state.value = 9;
                return StateUpdateResult::CHANGED;
            },
            "second");
        secondFinished.store(true);
    });

    while (!secondStarted.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    const bool secondBlockedBeforeRelease = !secondFinished.load();

    service.releaseHandler();

    first.join();
    second.join();

    TEST_ASSERT_TRUE(secondBlockedBeforeRelease);
    TEST_ASSERT_EQUAL(StateUpdateResult::CHANGED, firstResult.outcome);
    TEST_ASSERT_EQUAL(StateUpdateResult::CHANGED, secondResult.outcome);

    TestState snapshot{};
    service.read([&](TestState& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(9, snapshot.value);
    TEST_ASSERT_TRUE(snapshot.persisted);
}

void test_notification_settings_roll_back_to_synced_store_snapshot() {
    TEST_ASSERT_TRUE(NOTIFICATIONS::CONFIG_STORE::update([](RTC::NotificationData& state) {
        state = RTC::NotificationData{};
        state.telegramEnabled = false;
        state.webhookEnabled = false;
    }));

    PsychicHttpServer server;
    SecurityManager securityManager;
    NotificationSettingsService service(&server, nullptr, &securityManager);

    TEST_ASSERT_TRUE(NOTIFICATIONS::CONFIG_STORE::update([](RTC::NotificationData& state) {
        state.telegramEnabled = true;
        state.webhookEnabled = false;
    }));

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::NotificationData& state) {
            state.webhookEnabled = true;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);

    const RTC::NotificationData snapshot = NOTIFICATIONS::CONFIG_STORE::copy();
    TEST_ASSERT_TRUE(snapshot.telegramEnabled);
    TEST_ASSERT_FALSE(snapshot.webhookEnabled);
}

void test_udp_settings_service_rolls_back_store_on_save_failure() {
    RTC::mockStore.udpPusher = RTC::UdpPusherData{};
    copyUdpHost(RTC::mockStore.udpPusher.host, "initial.local");

    FS fs;
    UDPPUSH::UdpSettingsService service(&fs);
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::UdpPusherData& state) {
            state.enabled = true;
            state.port = 9000;
            state.intervalMs = 30000;
            copyUdpHost(state.host, "updated.local");
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_FALSE(RTC::getConfig().udpPusher.enabled);
    TEST_ASSERT_EQUAL(8094, RTC::getConfig().udpPusher.port);
    TEST_ASSERT_EQUAL(60000, RTC::getConfig().udpPusher.intervalMs);
    TEST_ASSERT_EQUAL_STRING("initial.local", RTC::getConfig().udpPusher.host);

    RTC::UdpPusherData snapshot{};
    service.read([&](RTC::UdpPusherData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.enabled);
    TEST_ASSERT_EQUAL_STRING("initial.local", snapshot.host);
}

void test_compensation_settings_service_rolls_back_store_on_save_failure() {
    RTC::mockStore.compensation = RTC::CompensationData{};
    RTC::mockStore.compensation.enabled = false;
    RTC::mockStore.compensation.baseTempOffset = 1.5f;
    RTC::mockStore.compensation.referenceCpuTemp = 32.0f;

    FS fs;
    COMPENSATION::CompensationSettingsService service(&fs, nullptr);
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::CompensationData& state) {
            state.enabled = true;
            state.baseTempOffset = 4.0f;
            state.referenceCpuTemp = 45.0f;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_FALSE(RTC::getConfig().compensation.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, RTC::getConfig().compensation.baseTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, RTC::getConfig().compensation.referenceCpuTemp);

    RTC::CompensationData snapshot{};
    service.read([&](RTC::CompensationData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.enabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.5f, snapshot.baseTempOffset);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 32.0f, snapshot.referenceCpuTemp);
}

void test_macro_settings_service_rolls_back_store_on_save_failure() {
    RTC::mockStore.macros = RTC::MacroData{};
    RTC::mockStore.macros.enabled = false;
    RTC::mockStore.macros.bootDelay = 5000;
    strncpy(RTC::mockStore.macros.bootScript, "boot-old.txt", sizeof(RTC::mockStore.macros.bootScript));
    RTC::mockStore.macros.bootScript[sizeof(RTC::mockStore.macros.bootScript) - 1] = '\0';

    FS fs;
    MACROS::MacroSettingsService service(&fs, nullptr);
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::MacroData& state) {
            state.enabled = true;
            state.bootDelay = 9000;
            strncpy(state.bootScript, "boot-new.txt", sizeof(state.bootScript));
            state.bootScript[sizeof(state.bootScript) - 1] = '\0';
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_FALSE(RTC::getConfig().macros.enabled);
    TEST_ASSERT_EQUAL(5000, RTC::getConfig().macros.bootDelay);
    TEST_ASSERT_EQUAL_STRING("boot-old.txt", RTC::getConfig().macros.bootScript);

    RTC::MacroData snapshot{};
    service.read([&](RTC::MacroData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.enabled);
    TEST_ASSERT_EQUAL(5000, snapshot.bootDelay);
    TEST_ASSERT_EQUAL_STRING("boot-old.txt", snapshot.bootScript);
}

void test_usb_terminal_settings_service_rolls_back_store_on_save_failure() {
    RTC::mockStore.usbTerminal = RTC::UsbTerminalData{};
    RTC::mockStore.usbTerminal.enabled = false;
    RTC::mockStore.usbTerminal.idleTimeoutMs = 2000;
    strncpy(RTC::mockStore.usbTerminal.targetPort, "/dev/ttyUSB0", sizeof(RTC::mockStore.usbTerminal.targetPort));
    RTC::mockStore.usbTerminal.targetPort[sizeof(RTC::mockStore.usbTerminal.targetPort) - 1] = '\0';

    FS fs;
    USB_TERMINAL::UsbTerminalSettingsService service(&fs);
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::UsbTerminalData& state) {
            state.enabled = true;
            state.idleTimeoutMs = 4000;
            strncpy(state.targetPort, "/dev/ttyUSB9", sizeof(state.targetPort));
            state.targetPort[sizeof(state.targetPort) - 1] = '\0';
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_FALSE(RTC::getConfig().usbTerminal.enabled);
    TEST_ASSERT_EQUAL(2000, RTC::getConfig().usbTerminal.idleTimeoutMs);
    TEST_ASSERT_EQUAL_STRING("/dev/ttyUSB0", RTC::getConfig().usbTerminal.targetPort);

    RTC::UsbTerminalData snapshot{};
    service.read([&](RTC::UsbTerminalData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.enabled);
    TEST_ASSERT_EQUAL(2000, snapshot.idleTimeoutMs);
    TEST_ASSERT_EQUAL_STRING("/dev/ttyUSB0", snapshot.targetPort);
}

void test_keyboard_settings_service_rolls_back_store_on_save_failure() {
    RTC::mockStore.keyboard = RTC::KeyboardData{};
    RTC::mockStore.keyboard.enabled = false;

    FS fs;
    KEYBOARD::KeyboardSettingsService service(&fs);
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::KeyboardData& state) {
            state.enabled = true;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_FALSE(RTC::getConfig().keyboard.enabled);

    RTC::KeyboardData snapshot{};
    service.read([&](RTC::KeyboardData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.enabled);
}

void test_heartbeat_settings_service_rolls_back_store_on_save_failure() {
    TEST_ASSERT_TRUE(SYSTEM::HEARTBEAT_CONFIG::update([](RTC::HeartbeatData& state) {
        state = RTC::HeartbeatData{};
        state.intervalMs = 300000;
    }));

    FS fs;
    SYSTEM::HeartbeatSettingsService service(&fs, []() {});
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::HeartbeatData& state) {
            state.intervalMs = 60000;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_EQUAL(300000, SYSTEM::HEARTBEAT_CONFIG::copy().intervalMs);

    RTC::HeartbeatData snapshot{};
    service.read([&](RTC::HeartbeatData& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(300000, snapshot.intervalMs);
}

void test_airmouse_settings_service_rolls_back_store_on_save_failure() {
    RTC::mockStore.airMouse = RTC::AirMouseData{};
    RTC::mockStore.airMouse.movementEnabled = true;
    RTC::mockStore.airMouse.clickEnabled = true;
    RTC::mockStore.airMouse.sensitivityX = 1.25f;

    FS fs;
    AIRMOUSE::AirMouseSettingsService service(&fs, []() {});
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::AirMouseData& state) {
            state.movementEnabled = false;
            state.clickEnabled = false;
            state.sensitivityX = 3.0f;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_TRUE(RTC::getConfig().airMouse.movementEnabled);
    TEST_ASSERT_TRUE(RTC::getConfig().airMouse.clickEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, RTC::getConfig().airMouse.sensitivityX);

    RTC::AirMouseData snapshot{};
    service.read([&](RTC::AirMouseData& state) {
        snapshot = state;
    });
    TEST_ASSERT_TRUE(snapshot.movementEnabled);
    TEST_ASSERT_TRUE(snapshot.clickEnabled);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, snapshot.sensitivityX);
}

void test_power_settings_service_rolls_back_store_on_apply_failure() {
    RTC::mockStore.power = RTC::PowerData{};
    RTC::mockStore.power.sleepEnabled = true;
    RTC::mockStore.power.inactivityTimeoutMs = 600000;
    RTC::mockStore.power.graceAfterBootMs = 120000;

    int applyCalls = 0;
    POWER::PowerSettingsService service([&](const RTC::PowerData& state) {
        ++applyCalls;
        if (applyCalls == 1) {
            return false;
        }

        RTC::mockStore.power = state;
        return true;
    });

    const StateTransactionResult result = service.updateAndPropagate(
        [](RTC::PowerData& state) {
            state.sleepEnabled = false;
            state.inactivityTimeoutMs = 900000;
            state.graceAfterBootMs = 90000;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/apply_failed", result.errorCode);
    TEST_ASSERT_TRUE(RTC::getConfig().power.sleepEnabled);
    TEST_ASSERT_EQUAL(600000, RTC::getConfig().power.inactivityTimeoutMs);
    TEST_ASSERT_EQUAL(120000, RTC::getConfig().power.graceAfterBootMs);
    TEST_ASSERT_EQUAL(2, applyCalls);

    RTC::PowerData snapshot{};
    service.read([&](RTC::PowerData& state) {
        snapshot = state;
    });
    TEST_ASSERT_TRUE(snapshot.sleepEnabled);
    TEST_ASSERT_EQUAL(600000, snapshot.inactivityTimeoutMs);
    TEST_ASSERT_EQUAL(120000, snapshot.graceAfterBootMs);
}

void test_power_settings_service_rejects_sleep_without_wake_source() {
    RTC::mockStore.power = RTC::PowerData{};
    RTC::mockStore.power.sleepEnabled = false;
    RTC::mockStore.power.wakeTimerEnabled = true;
    RTC::mockStore.power.wakeButtonEnabled = false;
    RTC::mockStore.power.wakeTouchEnabled = false;

    int applyCalls = 0;
    POWER::PowerSettingsService service([&](const RTC::PowerData& state) {
        ++applyCalls;
        RTC::mockStore.power = state;
        return true;
    });

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kSleepEnabled] = true;
    root[CONFIG::Keys::kWakeTimerEnabled] = false;
    root[CONFIG::Keys::kWakeButtonEnabled] = false;
    root[CONFIG::Keys::kWakeTouchEnabled] = false;

    const StateTransactionResult result = service.updateAndPropagate(
        root,
        POWER::PowerSettingsService::updateState,
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("internal/update_failed", result.errorCode);
    TEST_ASSERT_EQUAL(0, applyCalls);
    TEST_ASSERT_FALSE(RTC::getConfig().power.sleepEnabled);
    TEST_ASSERT_TRUE(RTC::getConfig().power.wakeTimerEnabled);

    RTC::PowerData snapshot{};
    service.read([&](RTC::PowerData& state) {
        snapshot = state;
    });
    TEST_ASSERT_FALSE(snapshot.sleepEnabled);
    TEST_ASSERT_TRUE(snapshot.wakeTimerEnabled);
    TEST_ASSERT_FALSE(snapshot.wakeButtonEnabled);
    TEST_ASSERT_FALSE(snapshot.wakeTouchEnabled);
}

void test_power_settings_validation_reports_missing_wake_source() {
    RTC::mockStore.power = RTC::PowerData{};
    RTC::mockStore.power.sleepEnabled = false;
    RTC::mockStore.power.wakeTimerEnabled = true;
    RTC::mockStore.power.wakeButtonEnabled = false;
    RTC::mockStore.power.wakeTouchEnabled = false;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kSleepEnabled] = true;
    root[CONFIG::Keys::kWakeTimerEnabled] = false;
    root[CONFIG::Keys::kWakeButtonEnabled] = false;
    root[CONFIG::Keys::kWakeTouchEnabled] = false;

    const StateHandlerResult result = POWER::PowerSettingsService::validateStateUpdate(root);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(400, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("input/power_wake_source_required", result.errorCode);
}

void test_alarm_settings_service_rolls_back_store_and_fs_on_apply_failure() {
    RTC::AlarmRulesData initialState{};
    initialState.ruleCount = 1;
    strncpy(initialState.rules[0].id, "alarm-old", sizeof(initialState.rules[0].id));
    initialState.rules[0].id[sizeof(initialState.rules[0].id) - 1] = '\0';
    strncpy(initialState.rules[0].name, "Old rule", sizeof(initialState.rules[0].name));
    initialState.rules[0].name[sizeof(initialState.rules[0].name) - 1] = '\0';
    initialState.rules[0].enabled = true;
    ALARMS::RULES_CONFIG::alarmRulesState = initialState;

    int applyCalls = 0;
    FS fs;
    ALARMS::AlarmSettingsService service(&fs, [&](const RTC::AlarmRulesData& state) {
        ++applyCalls;
        if (applyCalls == 1) {
            return false;
        }
        return state.ruleCount == initialState.ruleCount &&
            strcmp(state.rules[0].id, initialState.rules[0].id) == 0;
    });

    const StateTransactionResult result = service.updateAndPropagate(
        [&](RTC::AlarmRulesData& state) {
            state = RTC::AlarmRulesData{};
            state.ruleCount = 1;
            strncpy(state.rules[0].id, "alarm-new", sizeof(state.rules[0].id));
            state.rules[0].id[sizeof(state.rules[0].id) - 1] = '\0';
            strncpy(state.rules[0].name, "New rule", sizeof(state.rules[0].name));
            state.rules[0].name[sizeof(state.rules[0].name) - 1] = '\0';
            state.rules[0].enabled = false;
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/apply_failed", result.errorCode);
    TEST_ASSERT_EQUAL(2, applyCalls);
    TEST_ASSERT_EQUAL(2, CONFIG::saveAlarmRulesCallCount);
    TEST_ASSERT_EQUAL(1, ALARMS::RULES_CONFIG::alarmRulesState.ruleCount);
    TEST_ASSERT_EQUAL_STRING("alarm-old", ALARMS::RULES_CONFIG::alarmRulesState.rules[0].id);
    TEST_ASSERT_TRUE(ALARMS::RULES_CONFIG::alarmRulesState.rules[0].enabled);
    TEST_ASSERT_EQUAL(1, CONFIG::lastSavedAlarmRules.ruleCount);
    TEST_ASSERT_EQUAL_STRING("alarm-old", CONFIG::lastSavedAlarmRules.rules[0].id);
    TEST_ASSERT_TRUE(CONFIG::lastSavedAlarmRules.rules[0].enabled);

    RTC::AlarmRulesData snapshot{};
    service.read([&](RTC::AlarmRulesData& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(1, snapshot.ruleCount);
    TEST_ASSERT_EQUAL_STRING("alarm-old", snapshot.rules[0].id);
    TEST_ASSERT_TRUE(snapshot.rules[0].enabled);
}

void test_alarm_settings_rollback_reapplies_runtime_when_fs_restore_fails() {
    RTC::AlarmRulesData initialState{};
    initialState.ruleCount = 1;
    strncpy(initialState.rules[0].id,
            "alarm-old",
            sizeof(initialState.rules[0].id));
    strncpy(initialState.rules[0].name,
            "Old rule",
            sizeof(initialState.rules[0].name));
    initialState.rules[0].enabled = true;
    ALARMS::RULES_CONFIG::alarmRulesState = initialState;

    int applyCalls = 0;
    RTC::AlarmRulesData lastApplied{};
    FS fs;
    ALARMS::AlarmSettingsService service(
        &fs,
        [&](const RTC::AlarmRulesData& state) {
            ++applyCalls;
            lastApplied = state;
            return applyCalls != 1;
        });
    CONFIG::failSaveAlarmRulesOnCall = 2;

    const StateTransactionResult result = service.updateAndPropagate(
        [&](RTC::AlarmRulesData& state) {
            state = RTC::AlarmRulesData{};
            state.ruleCount = 1;
            strncpy(state.rules[0].id,
                    "alarm-new",
                    sizeof(state.rules[0].id));
            strncpy(state.rules[0].name,
                    "New rule",
                    sizeof(state.rules[0].name));
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(2, CONFIG::saveAlarmRulesCallCount);
    TEST_ASSERT_EQUAL(2, applyCalls);
    TEST_ASSERT_EQUAL_STRING("alarm-old", lastApplied.rules[0].id);
    TEST_ASSERT_EQUAL_STRING(
        "alarm-old", ALARMS::RULES_CONFIG::alarmRulesState.rules[0].id);
}

void test_alarm_settings_validation_reports_duplicate_rule_ids() {
    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kSchemaVersion] = 1;
    JsonArray rules = root[CONFIG::Keys::kRules].to<JsonArray>();

    JsonObject firstRule = rules.add<JsonObject>();
    firstRule[CONFIG::Keys::kId] = "dup-rule";
    firstRule[CONFIG::Keys::kName] = "First";
    firstRule[CONFIG::Keys::kEnabled] = true;

    JsonObject secondRule = rules.add<JsonObject>();
    secondRule[CONFIG::Keys::kId] = "dup-rule";
    secondRule[CONFIG::Keys::kName] = "Second";
    secondRule[CONFIG::Keys::kEnabled] = false;

    const StateHandlerResult result = ALARMS::AlarmSettingsService::validateStateUpdate(root);

    TEST_ASSERT_FALSE(result.ok);
    TEST_ASSERT_EQUAL(400, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("input/alarm_rules_duplicate_id", result.errorCode);
}

void test_alarm_settings_update_state_returns_unchanged_for_identical_payload() {
    RTC::AlarmRulesData state{};
    state.ruleCount = 1;
    state.rules[0] = ALARMS::AlarmRule{};
    strncpy(state.rules[0].id, "alarm-1", sizeof(state.rules[0].id));
    state.rules[0].id[sizeof(state.rules[0].id) - 1] = '\0';
    strncpy(state.rules[0].name, "Rule one", sizeof(state.rules[0].name));
    state.rules[0].name[sizeof(state.rules[0].name) - 1] = '\0';
    state.rules[0].enabled = true;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root[CONFIG::Keys::kSchemaVersion] = 1;
    JsonArray rules = root[CONFIG::Keys::kRules].to<JsonArray>();
    JsonObject rule = rules.add<JsonObject>();
    rule[CONFIG::Keys::kId] = "alarm-1";
    rule[CONFIG::Keys::kName] = "Rule one";
    rule[CONFIG::Keys::kEnabled] = true;

    const StateUpdateResult result = ALARMS::AlarmSettingsService::updateState(root, state, "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::UNCHANGED, result);
    TEST_ASSERT_EQUAL(1, state.ruleCount);
    TEST_ASSERT_EQUAL_STRING("alarm-1", state.rules[0].id);
    TEST_ASSERT_TRUE(state.rules[0].enabled);
}

void test_matrix_settings_service_rolls_back_rtc_and_icons_on_save_failure() {
    RTC::mockStore.matrix = RTC::MatrixData{};
    RTC::mockStore.matrix.brightness = 12;
    RTC::mockStore.matrix.effectEnabled = false;

    uint32_t originalIcon[MATRIX::kMatrixCustomIconPixels];
    uint32_t updatedIcon[MATRIX::kMatrixCustomIconPixels];
    for (size_t i = 0; i < MATRIX::kMatrixCustomIconPixels; i++) {
        originalIcon[i] = static_cast<uint32_t>(i + 1);
        updatedIcon[i] = static_cast<uint32_t>(1000 + i);
    }
    TEST_ASSERT_TRUE(MATRIX::setCustomIcon(0, originalIcon));

    FS fs;
    MatrixService matrixService;
    MATRIX::MatrixSettingsService service(&fs, &matrixService, nullptr, nullptr, nullptr);
    CONFIG::shouldSaveSucceed = false;

    const StateTransactionResult result = service.updateAndPropagate(
        [&](MATRIX::MatrixSettingsState& state) {
            state.config.brightness = 42;
            state.config.effectEnabled = true;
            state.customIcons.has[0] = true;
            memcpy(state.customIcons.icons[0], updatedIcon, sizeof(updatedIcon));
            return StateUpdateResult::CHANGED;
        },
        "test");

    TEST_ASSERT_EQUAL(StateUpdateResult::ERROR, result.outcome);
    TEST_ASSERT_EQUAL(500, result.httpStatus);
    TEST_ASSERT_EQUAL_STRING("config/save_failed", result.errorCode);
    TEST_ASSERT_EQUAL(12, RTC::getConfig().matrix.brightness);
    TEST_ASSERT_FALSE(RTC::getConfig().matrix.effectEnabled);

    uint32_t restoredIcon[MATRIX::kMatrixCustomIconPixels] = {};
    TEST_ASSERT_TRUE(MATRIX::copyCustomIcon(0, restoredIcon));
    TEST_ASSERT_EQUAL_UINT32_ARRAY(originalIcon, restoredIcon, MATRIX::kMatrixCustomIconPixels);

    MATRIX::MatrixSettingsState snapshot{};
    service.read([&](MATRIX::MatrixSettingsState& state) {
        snapshot = state;
    });
    TEST_ASSERT_EQUAL(12, snapshot.config.brightness);
    TEST_ASSERT_FALSE(snapshot.config.effectEnabled);
    TEST_ASSERT_TRUE(snapshot.customIcons.has[0]);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(originalIcon, snapshot.customIcons.icons[0], MATRIX::kMatrixCustomIconPixels);
}

void test_matrix_settings_service_heals_legacy_zero_and_one_brightness() {
    const uint8_t legacyValues[] = {0, 1, 2, 255};

    for (const uint8_t legacyValue : legacyValues) {
        RTC::mockStore.matrix = RTC::MatrixData{};
        RTC::mockStore.matrix.brightness = legacyValue;

        FS fs;
        MatrixService matrixService;
        MATRIX::MatrixSettingsService service(&fs, &matrixService, nullptr, nullptr, nullptr);

        MATRIX::MatrixSettingsState snapshot{};
        const StateHandlerResult readResult = service.read([&](MATRIX::MatrixSettingsState& state) {
            snapshot = state;
        });
        TEST_ASSERT_TRUE(readResult.ok);

        const uint8_t expected = legacyValue < UI::MATRIX::USER_BRIGHTNESS_MIN
            ? UI::MATRIX::USER_BRIGHTNESS_MIN
            : legacyValue;
        TEST_ASSERT_EQUAL_UINT8(expected, snapshot.config.brightness);

        service.begin();
        TEST_ASSERT_EQUAL_UINT8(expected, RTC::getConfig().matrix.brightness);
        TEST_ASSERT_EQUAL_UINT8(expected, service.getBrightness());
    }
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_http_endpoint_changed_returns_200_after_handlers_finish);
    RUN_TEST(test_http_endpoint_failed_handler_returns_500_and_rolls_back_state);
    RUN_TEST(test_http_endpoint_unchanged_skips_handlers_and_returns_current_state);
    RUN_TEST(test_http_endpoint_validator_failure_returns_error_without_updating_state);
    RUN_TEST(test_http_endpoint_post_notifies_activity_callback);
    RUN_TEST(test_http_endpoint_invalid_json_returns_structured_error);
    RUN_TEST(test_http_endpoint_payload_too_large_returns_structured_error);
    RUN_TEST(test_http_endpoint_validator_rejects_unexpected_fields);
    RUN_TEST(test_http_endpoint_get_returns_error_when_state_read_fails);
    RUN_TEST(test_rtc_stateful_service_rolls_back_store_on_handler_failure);
    RUN_TEST(test_rtc_stateful_service_failed_commit_restores_cached_state);
    RUN_TEST(test_stateful_service_update_without_propagation_rolls_back_on_error);
    RUN_TEST(test_rtc_stateful_service_update_without_propagation_restores_state_on_commit_failure);
    RUN_TEST(test_stateful_service_blocks_second_update_until_handlers_finish);
    RUN_TEST(test_notification_settings_roll_back_to_synced_store_snapshot);
    RUN_TEST(test_udp_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_compensation_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_macro_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_usb_terminal_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_keyboard_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_heartbeat_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_airmouse_settings_service_rolls_back_store_on_save_failure);
    RUN_TEST(test_power_settings_service_rolls_back_store_on_apply_failure);
    RUN_TEST(test_power_settings_service_rejects_sleep_without_wake_source);
    RUN_TEST(test_power_settings_validation_reports_missing_wake_source);
    RUN_TEST(test_alarm_settings_service_rolls_back_store_and_fs_on_apply_failure);
    RUN_TEST(test_alarm_settings_rollback_reapplies_runtime_when_fs_restore_fails);
    RUN_TEST(test_alarm_settings_validation_reports_duplicate_rule_ids);
    RUN_TEST(test_alarm_settings_update_state_returns_unchanged_for_identical_payload);
    RUN_TEST(test_matrix_settings_service_rolls_back_rtc_and_icons_on_save_failure);
    RUN_TEST(test_matrix_settings_service_heals_legacy_zero_and_one_brightness);
    return UNITY_END();
}

#endif
