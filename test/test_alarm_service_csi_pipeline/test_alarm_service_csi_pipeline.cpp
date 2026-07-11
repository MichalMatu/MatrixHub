#ifdef UNIT_TEST

#include <cstdarg>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <unity.h>

#include "../../src/alarms/AlarmService.h"

// Compile the real alarm pipeline into this native test. The native environment
// intentionally does not build production sources automatically.
#include "../../src/alarms/core/AlarmLogic.cpp"
#include "../../src/alarms/core/AlarmRuleManager.cpp"
#include "../../src/alarms/core/AlarmCoordinator.cpp"
#include "../../src/alarms/AlarmService.cpp"

namespace {

RTC::ConfigStore gRtcConfig{};
ALARMS::AlarmRulesSnapshot gStoredRules{};
int gMatrixUpdateCalls = 0;
int gMatrixClearCalls = 0;
ALARMS::AlarmAggregateState gLastMatrixAggregate{};
bool gRtcUpdateSucceeds = true;
bool gRtcReadSucceeds = true;
bool gRulesReadSucceeds = true;

struct ShellyCommand {
    std::string deviceId;
    bool turnOn = false;
};

std::mutex gShellyCommandsMutex;
std::vector<ShellyCommand> gShellyCommands;

struct ObservedChange {
    bool triggered = false;
    float currentValue = NAN;
};

ALARMS::AlarmRule makeCsiRule() {
    ALARMS::AlarmRule rule;
    std::strncpy(rule.id, "csi-motion", sizeof(rule.id) - 1);
    std::strncpy(rule.name, "CSI motion", sizeof(rule.name) - 1);
    rule.enabled = true;
    rule.source = ALARMS::AlarmSource::WifiCsiMotion;
    rule.op = ALARMS::AlarmOperator::Above;
    rule.threshold = 0.5f;
    rule.notifyChannels = ALARMS::NotifyChannel::None;
    rule.cooldownSeconds = 60;
    return rule;
}

ALARMS::AlarmRule makeTemperatureRule() {
    ALARMS::AlarmRule rule;
    std::strncpy(rule.id, "temperature", sizeof(rule.id) - 1);
    std::strncpy(rule.name, "Temperature", sizeof(rule.name) - 1);
    rule.enabled = true;
    rule.source = ALARMS::AlarmSource::Temperature;
    rule.op = ALARMS::AlarmOperator::Above;
    rule.threshold = 30.0f;
    rule.notifyChannels = ALARMS::NotifyChannel::Led;
    return rule;
}

ALARMS::AlarmRule makeImuRule() {
    ALARMS::AlarmRule rule;
    std::strncpy(rule.id, "imu-tamper", sizeof(rule.id) - 1);
    std::strncpy(rule.name, "IMU tamper", sizeof(rule.name) - 1);
    rule.enabled = true;
    rule.source = ALARMS::AlarmSource::ImuTamper;
    rule.notifyChannels = ALARMS::NotifyChannel::None;
    rule.cooldownSeconds = 60;
    rule.normalizeBooleanSemantics();
    return rule;
}

ALARMS::AlarmRule makeGpioRule(const char* ruleId, const char* gpioId) {
    ALARMS::AlarmRule rule;
    std::strncpy(rule.id, ruleId, sizeof(rule.id) - 1);
    std::strncpy(rule.name, ruleId, sizeof(rule.name) - 1);
    std::strncpy(rule.gpioId, gpioId, sizeof(rule.gpioId) - 1);
    rule.enabled = true;
    rule.source = ALARMS::AlarmSource::GpioDigital;
    rule.notifyChannels = ALARMS::NotifyChannel::None;
    rule.cooldownSeconds = 60;
    rule.normalizeBooleanSemantics();
    return rule;
}

void configureService(
    ALARMS::AlarmService& service,
    std::vector<ObservedChange>& observed) {
    const ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    service.setStateChangeCallback([&observed](const ALARMS::AlarmStateChange& change) {
        observed.push_back({change.triggered, change.currentValue});
    });
}

ALARMS::AlarmInputData csiInput(bool motion) {
    ALARMS::AlarmInputData input;
    input.wifiCsiMotion = motion ? 1.0f : 0.0f;
    return input;
}

ALARMS::AlarmInputData imuInput(bool tamper) {
    ALARMS::AlarmInputData input;
    input.imuTamper = tamper ? 1.0f : 0.0f;
    return input;
}

void installShellyRecorder(ALARMS::AlarmService& service) {
    service.setShellyActionExecutor([](const ALARMS::AlarmRule& rule, bool turnOn) {
        std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
        for (uint8_t i = 0;
             i < rule.shellyDeviceCount && i < ALARMS::kMaxShellyPerRule;
             ++i) {
            gShellyCommands.push_back({rule.shellyDeviceIds[i], turnOn});
        }
        return ALARMS::ShellyActionResult::Accepted;
    });
}

bool hasShellyCommand(const char* deviceId, bool turnOn) {
    std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
    for (const auto& command : gShellyCommands) {
        if (command.deviceId == deviceId && command.turnOn == turnOn) {
            return true;
        }
    }
    return false;
}

size_t shellyCommandCount() {
    std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
    return gShellyCommands.size();
}

void clearShellyCommands() {
    std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
    gShellyCommands.clear();
}

}  // namespace

// Minimal imperative-shell replacements. The assertions exercise the real
// AlarmService mailbox, AlarmCoordinator locking/evaluation, AlarmEvaluator,
// AlarmLogic and AlarmRuleManager runtime state. Display, notification, BLE,
// GPIO and retained-storage I/O are deliberately kept outside this test.
namespace LOG {

void Logging::log(esp_log_level_t, const char*, const char*, ...) {}

}  // namespace LOG

namespace RTC {

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    if (gRtcReadSucceeds) {
        reader(gRtcConfig);
    }
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    if (!gRtcUpdateSucceeds) {
        return false;
    }
    updater(gRtcConfig);
    return true;
}

}  // namespace RTC

namespace ALARMS::RULES_CONFIG {

void withRules(const std::function<void(const AlarmRulesSnapshot&)>& reader) {
    if (gRulesReadSucceeds) {
        reader(gStoredRules);
    }
}

}  // namespace ALARMS::RULES_CONFIG

namespace ALARMS {

AlarmMatrixController::AlarmMatrixController() = default;

void AlarmMatrixController::updateDisplay(bool, AlarmSeverity, const char*) {}

bool AlarmMatrixController::update(const AlarmAggregateState& aggregate) {
    gMatrixUpdateCalls++;
    gLastMatrixAggregate = aggregate;
    return true;
}

void AlarmMatrixController::reapplyLatchedState() {}

void AlarmMatrixController::clearLatchedState() {
    gMatrixClearCalls++;
}

bool AlarmMatrixController::isLatched() const {
    return false;
}

NotifyResult AlarmNotifier::notify(const EvaluationResult&) const {
    return {};
}

NotifyResult AlarmNotifier::notifyCleared(const EvaluationResult&) const {
    return {};
}

}  // namespace ALARMS

namespace BLE {

bool BleService::isRunning() const {
    return false;
}

bool BleService::getCachedDeviceData(
    const char*, float&, float&, uint8_t&, int8_t&, uint32_t&) const {
    return false;
}

}  // namespace BLE

namespace GPIO {

bool GpioService::getLogicalValue(const char*, bool&) const {
    return false;
}

}  // namespace GPIO

void setUp() {
    ALARMS::TEST_HOOKS::setAlarmPendingEventsAllocationFailure(false);
    ALARMS::TEST_HOOKS::setAlarmBootShellyAllocationFailure(false);
    gRtcConfig = RTC::ConfigStore{};
    gStoredRules = ALARMS::AlarmRulesSnapshot{};
    gMatrixUpdateCalls = 0;
    gMatrixClearCalls = 0;
    gLastMatrixAggregate.reset();
    gRtcUpdateSucceeds = true;
    gRtcReadSucceeds = true;
    gRulesReadSucceeds = true;
    {
        std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
        gShellyCommands.clear();
    }
    TEST_STUBS::ARDUINO::millisValue = 1000;
}

void tearDown() {
    ALARMS::TEST_HOOKS::setAlarmPendingEventsAllocationFailure(false);
    ALARMS::TEST_HOOKS::setAlarmBootShellyAllocationFailure(false);
}

void test_begin_fails_closed_when_pending_event_buffer_is_unavailable() {
    ALARMS::TEST_HOOKS::setAlarmPendingEventsAllocationFailure(true);

    ALARMS::AlarmService service(nullptr, nullptr);

    TEST_ASSERT_FALSE(service.begin());
    TEST_ASSERT_FALSE(service.getManager().isInitialized());
}

void test_begin_fails_closed_when_boot_shelly_buffer_is_unavailable() {
    ALARMS::TEST_HOOKS::setAlarmBootShellyAllocationFailure(true);

    ALARMS::AlarmService service(nullptr, nullptr);

    TEST_ASSERT_FALSE(service.begin());
    TEST_ASSERT_FALSE(service.getManager().isInitialized());
}

void test_true_then_false_before_processing_runs_rising_and_clear_passes_in_order() {
    ALARMS::AlarmService service(nullptr, nullptr);
    std::vector<ObservedChange> observed;
    configureService(service, observed);

    TEST_ASSERT_TRUE(service.submitInput(csiInput(true)));
    TEST_ASSERT_TRUE(service.submitInput(csiInput(false)));

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_TRUE(observed[0].triggered);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, observed[0].currentValue);
    TEST_ASSERT_TRUE(service.isSourceTriggered(ALARMS::AlarmSource::WifiCsiMotion));

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, observed.size());
    TEST_ASSERT_FALSE(observed[1].triggered);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, observed[1].currentValue);
    TEST_ASSERT_FALSE(service.isSourceTriggered(ALARMS::AlarmSource::WifiCsiMotion));

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, observed.size());
}

void test_imu_true_then_false_before_processing_preserves_tamper_edge() {
    ALARMS::AlarmService service(nullptr, nullptr);
    const ALARMS::AlarmRule rule = makeImuRule();
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    std::vector<ObservedChange> observed;
    service.setStateChangeCallback([&observed](const ALARMS::AlarmStateChange& change) {
        observed.push_back({change.triggered, change.currentValue});
    });

    TEST_ASSERT_TRUE(service.submitInput(imuInput(true)));
    TEST_ASSERT_TRUE(service.submitInput(imuInput(false)));

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_TRUE(observed[0].triggered);
    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, observed.size());
    TEST_ASSERT_FALSE(observed[1].triggered);
}

void test_gpio_pulse_between_alarm_passes_runs_rising_then_clear_in_order() {
    ALARMS::AlarmService service(nullptr, nullptr);
    const ALARMS::AlarmRule rule = makeGpioRule("door-rule", "gpio1");
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    std::vector<ObservedChange> observed;
    service.setStateChangeCallback([&observed](const ALARMS::AlarmStateChange& change) {
        observed.push_back({change.triggered, change.currentValue});
    });

    TEST_ASSERT_TRUE(service.submitGpioInput("gpio1", true));
    TEST_ASSERT_TRUE(service.submitGpioInput("gpio1", false));

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_TRUE(observed[0].triggered);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, observed[0].currentValue);

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, observed.size());
    TEST_ASSERT_FALSE(observed[1].triggered);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, observed[1].currentValue);

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, observed.size());
}

void test_gpio_edges_only_override_rules_with_matching_selector() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule rules[2] = {
        makeGpioRule("door-rule", "gpio1"),
        makeGpioRule("window-rule", "gpio2"),
    };
    TEST_ASSERT_TRUE(service.updateRules(rules, 2));

    TEST_ASSERT_TRUE(service.submitGpioInput("gpio1", true));
    service.processPending();
    TEST_ASSERT_TRUE(service.getManager().getStates()[0].previouslyTriggered);
    TEST_ASSERT_FALSE(service.getManager().getStates()[1].previouslyTriggered);

    TEST_ASSERT_TRUE(service.submitGpioInput("gpio2", true));
    service.processPending();
    TEST_ASSERT_TRUE(service.getManager().getStates()[0].previouslyTriggered);
    TEST_ASSERT_TRUE(service.getManager().getStates()[1].previouslyTriggered);

    TEST_ASSERT_TRUE(service.submitGpioInput("gpio1", false));
    service.processPending();
    TEST_ASSERT_FALSE(service.getManager().getStates()[0].previouslyTriggered);
    TEST_ASSERT_TRUE(service.getManager().getStates()[1].previouslyTriggered);
}

void test_gpio_edge_retries_after_coordinator_lock_timeout() {
    ALARMS::AlarmService service(nullptr, nullptr);
    const ALARMS::AlarmRule rule = makeGpioRule("door-rule", "gpio1");
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    TEST_ASSERT_TRUE(service.submitGpioInput("gpio1", true));

    SemaphoreHandle_t managerMutex = service.getManager().getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(managerMutex, 0));
    service.processPending();
    TEST_ASSERT_FALSE(service.getManager().getStates()[0].previouslyTriggered);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(managerMutex));

    service.processPending();
    TEST_ASSERT_TRUE(service.getManager().getStates()[0].previouslyTriggered);
}

void test_failed_coordinator_pass_remains_pending_until_mutex_recovers() {
    ALARMS::AlarmService service(nullptr, nullptr);
    std::vector<ObservedChange> observed;
    configureService(service, observed);
    TEST_ASSERT_TRUE(service.submitInput(csiInput(true)));

    SemaphoreHandle_t managerMutex = service.getManager().getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(managerMutex, 0));
    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(0, observed.size());
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(managerMutex));

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_TRUE(observed[0].triggered);
    TEST_ASSERT_TRUE(service.isSourceTriggered(ALARMS::AlarmSource::WifiCsiMotion));
}

void test_rising_edge_rtc_failure_rolls_back_and_retries_exactly_once() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    std::vector<ObservedChange> observed;

    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-rising-retry"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    clearShellyCommands();
    service.setStateChangeCallback([&observed](const ALARMS::AlarmStateChange& change) {
        observed.push_back({change.triggered, change.currentValue});
    });
    gMatrixUpdateCalls = 0;

    TEST_ASSERT_TRUE(service.submitInput(csiInput(true)));
    gRtcUpdateSucceeds = false;
    service.processPending();

    const ALARMS::AlarmRuntimeState& rolledBack = service.getManager().getStates()[0];
    TEST_ASSERT_FALSE(rolledBack.previouslyTriggered);
    TEST_ASSERT_FALSE(rolledBack.initialized);
    TEST_ASSERT_EQUAL_UINT32(0, observed.size());
    TEST_ASSERT_EQUAL_UINT32(0, shellyCommandCount());
    TEST_ASSERT_EQUAL_UINT32(0, gMatrixUpdateCalls);

    gRtcUpdateSucceeds = true;
    service.processPending();

    const ALARMS::AlarmRuntimeState& recovered = service.getManager().getStates()[0];
    TEST_ASSERT_TRUE(recovered.previouslyTriggered);
    TEST_ASSERT_TRUE(recovered.initialized);
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_TRUE(observed[0].triggered);
    TEST_ASSERT_TRUE(hasShellyCommand("relay-rising-retry", true));
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);
}

void test_clear_edge_rtc_failure_rolls_back_and_retries_exactly_once() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    std::vector<ObservedChange> observed;

    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-clear-retry"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    clearShellyCommands();
    service.setStateChangeCallback([&observed](const ALARMS::AlarmStateChange& change) {
        observed.push_back({change.triggered, change.currentValue});
    });

    ALARMS::AlarmRuntimeState& active = service.getManager().getStates()[0];
    active.previouslyTriggered = true;
    active.initialized = true;
    active.lastTriggeredMs = 900;
    active.lastValue = 1.0f;
    TEST_ASSERT_TRUE(service.getManager().persistRuntimeState());
    TEST_ASSERT_TRUE(gRtcConfig.alarms.runtimeStates[0].previouslyTriggered);
    gMatrixUpdateCalls = 0;

    TEST_ASSERT_TRUE(service.submitInput(csiInput(false)));
    gRtcUpdateSucceeds = false;
    service.processPending();

    const ALARMS::AlarmRuntimeState& rolledBack = service.getManager().getStates()[0];
    TEST_ASSERT_TRUE(rolledBack.previouslyTriggered);
    TEST_ASSERT_TRUE(rolledBack.initialized);
    TEST_ASSERT_EQUAL_UINT32(900, rolledBack.lastTriggeredMs);
    TEST_ASSERT_TRUE(gRtcConfig.alarms.runtimeStates[0].previouslyTriggered);
    TEST_ASSERT_EQUAL_UINT32(0, observed.size());
    TEST_ASSERT_EQUAL_UINT32(0, shellyCommandCount());
    TEST_ASSERT_EQUAL_UINT32(0, gMatrixUpdateCalls);

    gRtcUpdateSucceeds = true;
    service.processPending();

    const ALARMS::AlarmRuntimeState& recovered = service.getManager().getStates()[0];
    TEST_ASSERT_FALSE(recovered.previouslyTriggered);
    TEST_ASSERT_TRUE(recovered.initialized);
    TEST_ASSERT_EQUAL_UINT32(0, recovered.lastTriggeredMs);
    TEST_ASSERT_FALSE(gRtcConfig.alarms.runtimeStates[0].previouslyTriggered);
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_FALSE(observed[0].triggered);
    TEST_ASSERT_TRUE(hasShellyCommand("relay-clear-retry", false));
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);

    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);
}

void test_retained_triggered_state_is_cleared_by_first_definitive_false() {
    ALARMS::AlarmService service(nullptr, nullptr);
    std::vector<ObservedChange> observed;
    configureService(service, observed);

    SemaphoreHandle_t managerMutex = service.getManager().getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(managerMutex, portMAX_DELAY));
    ALARMS::AlarmRuntimeState& retained = service.getManager().getStates()[0];
    retained.previouslyTriggered = true;
    retained.initialized = true;
    retained.lastTriggeredMs = 123;
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(managerMutex));

    TEST_ASSERT_TRUE(service.submitInput(csiInput(false)));
    service.processPending();

    TEST_ASSERT_EQUAL_UINT32(1, observed.size());
    TEST_ASSERT_FALSE(observed[0].triggered);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, observed[0].currentValue);
    TEST_ASSERT_FALSE(service.isSourceTriggered(ALARMS::AlarmSource::WifiCsiMotion));
}

void test_begin_reanchors_retained_active_cooldown_without_boot_reminder() {
    ALARMS::AlarmRule rule = makeCsiRule();
    rule.notifyChannels = ALARMS::NotifyChannel::Led;
    gStoredRules.rules[0] = rule;
    gStoredRules.ruleCount = 1;

    ALARMS::AlarmRuntimeState& rtcState = gRtcConfig.alarms.runtimeStates[0];
    rtcState.previouslyTriggered = true;
    rtcState.initialized = true;
    rtcState.lastTriggeredMs = 0xFFFFFF00;
    gRtcConfig.alarms.ruleCount = 1;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[0] =
        ALARMS::stableAlarmRuntimeIdentityHash(rule);

    ALARMS::AlarmService service(nullptr, nullptr);
    TEST_ASSERT_TRUE(service.begin());

    SemaphoreHandle_t managerMutex = service.getManager().getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(managerMutex, portMAX_DELAY));
    ALARMS::AlarmRuntimeState& restored = service.getManager().getStates()[0];

    TEST_ASSERT_TRUE(restored.previouslyTriggered);
    TEST_ASSERT_TRUE(restored.initialized);
    TEST_ASSERT_EQUAL_UINT32(0, restored.lastTriggeredMs);
    TEST_ASSERT_EQUAL_UINT32(0, gRtcConfig.alarms.runtimeStates[0].lastTriggeredMs);
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);
    TEST_ASSERT_TRUE(gLastMatrixAggregate.active);

    const ALARMS::EvaluationResult first =
        ALARMS::AlarmEvaluator::evaluate(rule, csiInput(true), restored, 1000);
    TEST_ASSERT_TRUE(first.triggered);
    TEST_ASSERT_FALSE(first.stateChanged);
    TEST_ASSERT_FALSE(first.shouldNotify);
    TEST_ASSERT_EQUAL_UINT32(1000, restored.lastTriggeredMs);
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(managerMutex));
}

void test_begin_fails_closed_when_boot_runtime_commit_fails() {
    const ALARMS::AlarmRule rule = makeCsiRule();
    gStoredRules.rules[0] = rule;
    gStoredRules.ruleCount = 1;
    gRtcConfig.alarms.ruleCount = 1;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[0] =
        ALARMS::stableAlarmRuntimeIdentityHash(rule);
    gRtcConfig.alarms.runtimeStates[0].previouslyTriggered = true;
    gRtcConfig.alarms.runtimeStates[0].initialized = true;
    gRtcUpdateSucceeds = false;

    ALARMS::AlarmService service(nullptr, nullptr);
    TEST_ASSERT_FALSE(service.begin());
    TEST_ASSERT_TRUE(service.getManager().getStates()[0].previouslyTriggered);
    TEST_ASSERT_TRUE(service.getManager().getStates()[0].initialized);
}

void test_begin_fails_closed_when_boot_snapshot_read_is_incomplete() {
    const ALARMS::AlarmRule rule = makeCsiRule();
    gStoredRules.rules[0] = rule;
    gStoredRules.ruleCount = 1;
    gRtcConfig.alarms.ruleCount = 1;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[0] =
        ALARMS::stableAlarmRuntimeIdentityHash(rule);

    gRtcReadSucceeds = false;
    ALARMS::AlarmService rtcReadFailure(nullptr, nullptr);
    TEST_ASSERT_FALSE(rtcReadFailure.begin());
    TEST_ASSERT_FALSE(rtcReadFailure.getManager().isInitialized());

    gRtcReadSucceeds = true;
    gRulesReadSucceeds = false;
    ALARMS::AlarmService rulesReadFailure(nullptr, nullptr);
    TEST_ASSERT_FALSE(rulesReadFailure.begin());
    TEST_ASSERT_FALSE(rulesReadFailure.getManager().isInitialized());
}

void test_begin_maps_retained_runtime_by_rule_identity_after_reorder() {
    const ALARMS::AlarmRule csiRule = makeCsiRule();
    const ALARMS::AlarmRule temperatureRule = makeTemperatureRule();
    gStoredRules.rules[0] = temperatureRule;
    gStoredRules.rules[1] = csiRule;
    gStoredRules.ruleCount = 2;

    gRtcConfig.alarms.ruleCount = 2;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[0] =
        ALARMS::stableAlarmRuntimeIdentityHash(csiRule);
    gRtcConfig.alarms.runtimeStates[0].previouslyTriggered = true;
    gRtcConfig.alarms.runtimeStates[0].initialized = true;
    gRtcConfig.alarms.runtimeStates[0].lastTriggeredMs = 444;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[1] =
        ALARMS::stableAlarmRuntimeIdentityHash(temperatureRule);

    ALARMS::AlarmService service(nullptr, nullptr);
    TEST_ASSERT_TRUE(service.begin());

    TEST_ASSERT_FALSE(service.getManager().getStates()[0].previouslyTriggered);
    TEST_ASSERT_TRUE(service.getManager().getStates()[1].previouslyTriggered);
    TEST_ASSERT_EQUAL_UINT32(0,
                             service.getManager().getStates()[1].lastTriggeredMs);
    TEST_ASSERT_EQUAL_UINT64(
        ALARMS::stableAlarmRuntimeIdentityHash(temperatureRule),
        gRtcConfig.alarms.ruleRuntimeIdentityHashes[0]);
    TEST_ASSERT_EQUAL_UINT64(
        ALARMS::stableAlarmRuntimeIdentityHash(csiRule),
        gRtcConfig.alarms.ruleRuntimeIdentityHashes[1]);
}

void test_begin_rejects_retained_runtime_after_torn_semantic_update() {
    const ALARMS::AlarmRule oldRule = makeTemperatureRule();
    ALARMS::AlarmRule persistedNewRule = oldRule;
    persistedNewRule.threshold = 31.0f;
    gStoredRules.rules[0] = persistedNewRule;
    gStoredRules.ruleCount = 1;

    // Simulate power loss after LittleFS stored the new rule but before the RTC
    // runtime transaction reset the old active state.
    gRtcConfig.alarms.ruleCount = 1;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[0] =
        ALARMS::stableAlarmRuntimeIdentityHash(oldRule);
    gRtcConfig.alarms.runtimeStates[0].previouslyTriggered = true;
    gRtcConfig.alarms.runtimeStates[0].initialized = true;
    gRtcConfig.alarms.runtimeStates[0].lastTriggeredMs = 777;

    ALARMS::AlarmService service(nullptr, nullptr);
    TEST_ASSERT_TRUE(service.begin());

    const ALARMS::AlarmRuntimeState& restored = service.getManager().getStates()[0];
    TEST_ASSERT_FALSE(restored.previouslyTriggered);
    TEST_ASSERT_FALSE(restored.initialized);
    TEST_ASSERT_EQUAL_UINT32(0, restored.lastTriggeredMs);
    TEST_ASSERT_EQUAL_UINT64(
        ALARMS::stableAlarmRuntimeIdentityHash(persistedNewRule),
        gRtcConfig.alarms.ruleRuntimeIdentityHashes[0]);
}

void test_executor_install_reconciles_retained_global_shelly_or_once_per_device() {
    ALARMS::AlarmRule active = makeCsiRule();
    std::strncpy(active.id, "active-rule", sizeof(active.id) - 1);
    TEST_ASSERT_TRUE(active.addShellyDevice("relay-shared"));

    ALARMS::AlarmRule inactive = makeCsiRule();
    std::strncpy(inactive.id, "inactive-rule", sizeof(inactive.id) - 1);
    TEST_ASSERT_TRUE(inactive.addShellyDevice("relay-shared"));
    TEST_ASSERT_TRUE(inactive.addShellyDevice("relay-off"));

    gStoredRules.rules[0] = active;
    gStoredRules.rules[1] = inactive;
    gStoredRules.ruleCount = 2;
    gRtcConfig.alarms.ruleCount = 2;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[0] =
        ALARMS::stableAlarmRuntimeIdentityHash(active);
    gRtcConfig.alarms.runtimeStates[0].previouslyTriggered = true;
    gRtcConfig.alarms.runtimeStates[0].initialized = true;
    gRtcConfig.alarms.ruleRuntimeIdentityHashes[1] =
        ALARMS::stableAlarmRuntimeIdentityHash(inactive);
    gRtcConfig.alarms.runtimeStates[1].previouslyTriggered = false;
    gRtcConfig.alarms.runtimeStates[1].initialized = true;

    ALARMS::AlarmService service(nullptr, nullptr);
    TEST_ASSERT_TRUE(service.begin());
    installShellyRecorder(service);

    TEST_ASSERT_TRUE(hasShellyCommand("relay-shared", true));
    TEST_ASSERT_TRUE(hasShellyCommand("relay-off", false));
    std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
    TEST_ASSERT_EQUAL_UINT32(2, gShellyCommands.size());
}

void test_unrelated_edit_and_reorder_preserve_retained_csi_state_by_id() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule initialRules[2] = {makeCsiRule(), makeTemperatureRule()};
    initialRules[0].notifyChannels = ALARMS::NotifyChannel::Led;
    TEST_ASSERT_TRUE(service.updateRules(initialRules, 2));

    SemaphoreHandle_t managerMutex = service.getManager().getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(managerMutex, portMAX_DELAY));
    ALARMS::AlarmRuntimeState& retained = service.getManager().getStates()[0];
    retained.previouslyTriggered = true;
    retained.initialized = true;
    retained.lastTriggeredMs = 777;
    retained.lastValue = 1.0f;
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(managerMutex));

    ALARMS::AlarmRule editedTemperature = makeTemperatureRule();
    std::strncpy(editedTemperature.name,
                 "Edited temperature",
                 sizeof(editedTemperature.name) - 1);
    ALARMS::AlarmRule editedCsi = makeCsiRule();
    editedCsi.notifyChannels = ALARMS::NotifyChannel::Led;
    std::strncpy(editedCsi.name, "Retained CSI", sizeof(editedCsi.name) - 1);
    editedCsi.severity = ALARMS::AlarmSeverity::Critical;
    // Incoming legacy/inverted boolean values are canonicalized without
    // manufacturing a false edge for an already retained CSI alarm.
    editedCsi.op = ALARMS::AlarmOperator::Below;
    editedCsi.threshold = 42.0f;
    ALARMS::AlarmRule reorderedRules[2] = {editedTemperature, editedCsi};

    gMatrixUpdateCalls = 0;
    gMatrixClearCalls = 0;
    TEST_ASSERT_TRUE(service.updateRules(reorderedRules, 2));

    TEST_ASSERT_EQUAL_STRING("temperature", service.getManager().getRules()[0].id);
    TEST_ASSERT_EQUAL_STRING("csi-motion", service.getManager().getRules()[1].id);
    const ALARMS::AlarmRuntimeState& migrated = service.getManager().getStates()[1];
    TEST_ASSERT_TRUE(migrated.previouslyTriggered);
    TEST_ASSERT_TRUE(migrated.initialized);
    TEST_ASSERT_EQUAL_UINT32(777, migrated.lastTriggeredMs);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, migrated.lastValue);
    TEST_ASSERT_EQUAL(ALARMS::AlarmOperator::Above,
                      service.getManager().getRules()[1].op);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.5f,
                             service.getManager().getRules()[1].threshold);
    TEST_ASSERT_TRUE(gRtcConfig.alarms.runtimeStates[1].previouslyTriggered);
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);
    TEST_ASSERT_EQUAL_UINT32(0, gMatrixClearCalls);
    TEST_ASSERT_TRUE(gLastMatrixAggregate.active);
    TEST_ASSERT_EQUAL(ALARMS::AlarmSeverity::Critical,
                      gLastMatrixAggregate.maxSeverity);
    TEST_ASSERT_EQUAL_STRING("Retained CSI", gLastMatrixAggregate.alarmName);
}

void test_explicit_csi_disable_resets_runtime_and_clears_aggregate() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));

    SemaphoreHandle_t managerMutex = service.getManager().getMutex();
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreTake(managerMutex, portMAX_DELAY));
    ALARMS::AlarmRuntimeState& retained = service.getManager().getStates()[0];
    retained.previouslyTriggered = true;
    retained.initialized = true;
    retained.lastTriggeredMs = 900;
    TEST_ASSERT_EQUAL(pdTRUE, xSemaphoreGive(managerMutex));

    rule.enabled = false;
    gMatrixUpdateCalls = 0;
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));

    const ALARMS::AlarmRuntimeState& reset = service.getManager().getStates()[0];
    TEST_ASSERT_FALSE(reset.previouslyTriggered);
    TEST_ASSERT_FALSE(reset.initialized);
    TEST_ASSERT_EQUAL_UINT32(0, reset.lastTriggeredMs);
    TEST_ASSERT_FALSE(service.isSourceTriggered(ALARMS::AlarmSource::WifiCsiMotion));
    TEST_ASSERT_EQUAL_UINT32(1, gMatrixUpdateCalls);
    TEST_ASSERT_FALSE(gLastMatrixAggregate.active);
}

void test_semantic_change_turns_off_old_active_shelly_binding() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-old"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));

    ALARMS::AlarmRuntimeState& state = service.getManager().getStates()[0];
    state.previouslyTriggered = true;
    state.initialized = true;

    ALARMS::AlarmRule changed = rule;
    changed.source = ALARMS::AlarmSource::Temperature;
    changed.threshold = 28.0f;
    TEST_ASSERT_TRUE(service.updateRules(&changed, 1));

    TEST_ASSERT_TRUE(hasShellyCommand("relay-old", false));
    TEST_ASSERT_FALSE(service.getManager().getStates()[0].previouslyTriggered);
}

void test_active_shelly_binding_diff_turns_removed_off_and_added_on() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-removed"));
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-kept"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    clearShellyCommands();

    ALARMS::AlarmRuntimeState& state = service.getManager().getStates()[0];
    state.previouslyTriggered = true;
    state.initialized = true;

    ALARMS::AlarmRule changed = makeCsiRule();
    TEST_ASSERT_TRUE(changed.addShellyDevice("relay-kept"));
    TEST_ASSERT_TRUE(changed.addShellyDevice("relay-added"));
    TEST_ASSERT_TRUE(service.updateRules(&changed, 1));

    TEST_ASSERT_TRUE(hasShellyCommand("relay-removed", false));
    TEST_ASSERT_TRUE(hasShellyCommand("relay-added", true));
    TEST_ASSERT_FALSE(hasShellyCommand("relay-kept", false));
}

void test_new_inactive_shelly_binding_is_explicitly_reconciled_off() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);

    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    {
        std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
        gShellyCommands.clear();
    }

    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-new-inactive"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));

    TEST_ASSERT_TRUE(hasShellyCommand("relay-new-inactive", false));
    TEST_ASSERT_FALSE(hasShellyCommand("relay-new-inactive", true));
}

void test_transient_shelly_admission_failure_retries_without_new_alarm_edge() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-retry"));

    // Queue reconciliation before the executor exists. Installing it performs
    // the first attempt, which models a lifecycle/DeviceManager timeout.
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    uint8_t failuresRemaining = 1;
    service.setShellyActionExecutor(
        [&failuresRemaining](const ALARMS::AlarmRule& actionRule, bool turnOn) {
            {
                std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
                gShellyCommands.push_back(
                    {actionRule.shellyDeviceIds[0], turnOn});
            }
            if (failuresRemaining > 0) {
                --failuresRemaining;
                return ALARMS::ShellyActionResult::Retry;
            }
            return ALARMS::ShellyActionResult::Accepted;
        });

    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
    TEST_ASSERT_TRUE(hasShellyCommand("relay-retry", false));

    // There is no sensor/CSI/GPIO input pending. The coordinator's in-memory
    // outbox is nevertheless retried from the regular processPending tick.
    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, shellyCommandCount());
    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(2, shellyCommandCount());
}

void test_pending_shelly_outbox_prevents_second_rule_commit_from_overflowing() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule first = makeCsiRule();
    TEST_ASSERT_TRUE(first.addShellyDevice("relay-pending"));

    // With no executor, the first committed update occupies the in-memory
    // imperative outbox. A second update must fail before manager commit rather
    // than append beyond fixed capacity and silently lose a binding intent.
    TEST_ASSERT_TRUE(service.updateRules(&first, 1));
    ALARMS::AlarmRule second = first;
    TEST_ASSERT_TRUE(second.addShellyDevice("relay-new"));
    TEST_ASSERT_FALSE(service.updateRules(&second, 1));

    TEST_ASSERT_EQUAL_UINT8(1, service.getManager().getCount());
    TEST_ASSERT_EQUAL_UINT8(
        1, service.getManager().getRules()[0].shellyDeviceCount);
    TEST_ASSERT_EQUAL_STRING(
        "relay-pending",
        service.getManager().getRules()[0].shellyDeviceIds[0]);
}

void test_full_shelly_outbox_rejects_new_intent_and_rule_commit() {
    ALARMS::AlarmService service(nullptr, nullptr);
    for (uint8_t i = 0; i < ALARMS::kMaxRuleUpdateShellyDevices; ++i) {
        char deviceId[ALARMS::kShellyIdLen];
        snprintf(deviceId, sizeof(deviceId), "pending-%u", i);
        TEST_ASSERT_TRUE(service.enqueueShellyReconcileForTest(deviceId));
    }
    TEST_ASSERT_EQUAL_UINT8(
        ALARMS::kMaxRuleUpdateShellyDevices,
        service.pendingShellyReconcileCountForTest());
    TEST_ASSERT_FALSE(
        service.enqueueShellyReconcileForTest("one-too-many"));

    const ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_FALSE(service.updateRules(&rule, 1));
    TEST_ASSERT_EQUAL_UINT8(0, service.getManager().getCount());
}

void test_terminal_missing_shelly_target_is_not_retried_forever() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-removed"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));

    service.setShellyActionExecutor(
        [](const ALARMS::AlarmRule& actionRule, bool turnOn) {
            std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
            gShellyCommands.push_back(
                {actionRule.shellyDeviceIds[0], turnOn});
            return ALARMS::ShellyActionResult::Terminal;
        });
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());

    service.processPending();
    service.processPending();
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
}

void test_shelly_config_reconcile_applies_active_alarm_when_missing_peer_is_added() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-later"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    service.getManager().getStates()[0].previouslyTriggered = true;
    service.getManager().getStates()[0].initialized = true;

    bool peerPresent = false;
    service.setShellyActionExecutor(
        [&peerPresent](const ALARMS::AlarmRule& actionRule, bool turnOn) {
            std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
            gShellyCommands.push_back(
                {actionRule.shellyDeviceIds[0], turnOn});
            return peerPresent ? ALARMS::ShellyActionResult::Accepted
                               : ALARMS::ShellyActionResult::Terminal;
        });
    TEST_ASSERT_EQUAL_UINT32(1, shellyCommandCount());
    TEST_ASSERT_TRUE(hasShellyCommand("relay-later", true));

    // ShellyService invokes this after add/re-add, outside its lifecycle lock.
    // The retained global OR is reasserted without any new sensor/alarm edge.
    peerPresent = true;
    service.reconcileShellyOutputs();
    TEST_ASSERT_EQUAL_UINT32(2, shellyCommandCount());
    TEST_ASSERT_TRUE(hasShellyCommand("relay-later", true));
}

void test_shared_shelly_binding_uses_global_or_when_one_rule_clears() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    ALARMS::AlarmRule csiRule = makeCsiRule();
    TEST_ASSERT_TRUE(csiRule.addShellyDevice("relay-shared"));
    ALARMS::AlarmRule temperatureRule = makeTemperatureRule();
    TEST_ASSERT_TRUE(temperatureRule.addShellyDevice("relay-shared"));
    ALARMS::AlarmRule rules[2] = {csiRule, temperatureRule};
    TEST_ASSERT_TRUE(service.updateRules(rules, 2));

    ALARMS::AlarmRuntimeState& otherActive = service.getManager().getStates()[1];
    otherActive.previouslyTriggered = true;
    otherActive.initialized = true;

    TEST_ASSERT_TRUE(service.submitInput(csiInput(true)));
    service.processPending();
    {
        std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
        gShellyCommands.clear();
    }
    TEST_ASSERT_TRUE(service.submitInput(csiInput(false)));
    service.processPending();

    TEST_ASSERT_TRUE(hasShellyCommand("relay-shared", true));
    TEST_ASSERT_FALSE(hasShellyCommand("relay-shared", false));
}

void test_removing_shared_binding_keeps_device_on_for_other_active_rule() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    ALARMS::AlarmRule first = makeCsiRule();
    std::strncpy(first.id, "csi-first", sizeof(first.id) - 1);
    std::strncpy(first.name, "CSI first", sizeof(first.name) - 1);
    TEST_ASSERT_TRUE(first.addShellyDevice("relay-shared"));
    ALARMS::AlarmRule second = makeCsiRule();
    std::strncpy(second.id, "csi-second", sizeof(second.id) - 1);
    std::strncpy(second.name, "CSI second", sizeof(second.name) - 1);
    TEST_ASSERT_TRUE(second.addShellyDevice("relay-shared"));
    ALARMS::AlarmRule rules[2] = {first, second};
    TEST_ASSERT_TRUE(service.updateRules(rules, 2));
    clearShellyCommands();
    service.getManager().getStates()[0].previouslyTriggered = true;
    service.getManager().getStates()[0].initialized = true;
    service.getManager().getStates()[1].previouslyTriggered = true;
    service.getManager().getStates()[1].initialized = true;

    first.clearShellyDevices();
    ALARMS::AlarmRule changed[2] = {first, second};
    TEST_ASSERT_TRUE(service.updateRules(changed, 2));

    TEST_ASSERT_TRUE(hasShellyCommand("relay-shared", true));
    TEST_ASSERT_FALSE(hasShellyCommand("relay-shared", false));
}

void test_pending_old_on_finishes_before_disable_reconciles_final_off() {
    ALARMS::AlarmService service(nullptr, nullptr);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-race"));

    std::atomic<bool> onEntered{false};
    std::atomic<bool> releaseOn{false};
    service.setShellyActionExecutor(
        [&](const ALARMS::AlarmRule& actionRule, bool turnOn) {
            {
                std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
                gShellyCommands.push_back(
                    {actionRule.shellyDeviceIds[0], turnOn});
            }
            if (turnOn) {
                onEntered.store(true, std::memory_order_release);
                while (!releaseOn.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            }
            return ALARMS::ShellyActionResult::Accepted;
        });
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    clearShellyCommands();
    TEST_ASSERT_TRUE(service.submitInput(csiInput(true)));

    std::thread processing([&]() { service.processPending(); });
    while (!onEntered.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    rule.enabled = false;
    std::atomic<bool> updateStarted{false};
    std::atomic<bool> updateCompleted{false};
    std::atomic<bool> updateResult{false};
    std::thread updating([&]() {
        updateStarted.store(true, std::memory_order_release);
        updateResult.store(service.updateRules(&rule, 1), std::memory_order_release);
        updateCompleted.store(true, std::memory_order_release);
    });
    while (!updateStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    TEST_ASSERT_FALSE(updateCompleted.load(std::memory_order_acquire));

    releaseOn.store(true, std::memory_order_release);
    processing.join();
    updating.join();
    TEST_ASSERT_TRUE(updateResult.load(std::memory_order_acquire));

    std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
    TEST_ASSERT_EQUAL_UINT32(2, gShellyCommands.size());
    TEST_ASSERT_TRUE(gShellyCommands[0].turnOn);
    TEST_ASSERT_FALSE(gShellyCommands[1].turnOn);
}

void test_rtc_commit_failure_restores_rules_without_shelly_effects() {
    ALARMS::AlarmService service(nullptr, nullptr);
    installShellyRecorder(service);
    ALARMS::AlarmRule rule = makeCsiRule();
    TEST_ASSERT_TRUE(rule.addShellyDevice("relay-rollback"));
    TEST_ASSERT_TRUE(service.updateRules(&rule, 1));
    clearShellyCommands();
    service.getManager().getStates()[0].previouslyTriggered = true;
    service.getManager().getStates()[0].initialized = true;

    gRtcUpdateSucceeds = false;
    rule.enabled = false;
    TEST_ASSERT_FALSE(service.updateRules(&rule, 1));

    TEST_ASSERT_TRUE(service.getManager().getRules()[0].enabled);
    TEST_ASSERT_TRUE(service.getManager().getStates()[0].previouslyTriggered);
    std::lock_guard<std::mutex> lock(gShellyCommandsMutex);
    TEST_ASSERT_EQUAL_UINT32(0, gShellyCommands.size());
}

void test_rule_update_rejects_duplicate_or_partial_input_without_mutation() {
    ALARMS::AlarmService service(nullptr, nullptr);
    const ALARMS::AlarmRule initial = makeCsiRule();
    TEST_ASSERT_TRUE(service.updateRules(&initial, 1));

    ALARMS::AlarmRule duplicates[2] = {makeCsiRule(), makeCsiRule()};
    std::strncpy(duplicates[1].name,
                 "Duplicate identity",
                 sizeof(duplicates[1].name) - 1);
    TEST_ASSERT_FALSE(service.updateRules(duplicates, 2));
    TEST_ASSERT_EQUAL_UINT8(1, service.getManager().getCount());
    TEST_ASSERT_EQUAL_STRING(
        "csi-motion", service.getManager().getRules()[0].id);

    ALARMS::AlarmRule invalid = makeTemperatureRule();
    invalid.name[0] = '\0';
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));
    TEST_ASSERT_FALSE(service.updateRules(nullptr, 1));

    invalid = makeTemperatureRule();
    invalid.source = static_cast<ALARMS::AlarmSource>(255);
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));

    invalid = makeTemperatureRule();
    invalid.notifyChannels = static_cast<ALARMS::NotifyChannel>(255);
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));

    invalid = makeTemperatureRule();
    invalid.cooldownSeconds = LIMITS::ALARMS::MAX_COOLDOWN_SEC + 1;
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));

    invalid = makeTemperatureRule();
    invalid.shellyDeviceCount = ALARMS::kMaxShellyPerRule + 1;
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));

    invalid = makeTemperatureRule();
    invalid.source = ALARMS::AlarmSource::BleTemperature;
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));

    invalid = makeTemperatureRule();
    invalid.source = ALARMS::AlarmSource::GpioDigital;
    TEST_ASSERT_FALSE(service.updateRules(&invalid, 1));

    ALARMS::AlarmRule duplicateNames[2] = {
        makeCsiRule(), makeTemperatureRule()};
    std::strncpy(duplicateNames[1].name,
                 " csi MOTION ",
                 sizeof(duplicateNames[1].name) - 1);
    TEST_ASSERT_FALSE(service.updateRules(duplicateNames, 2));
    TEST_ASSERT_EQUAL_UINT8(1, service.getManager().getCount());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_begin_fails_closed_when_pending_event_buffer_is_unavailable);
    RUN_TEST(test_begin_fails_closed_when_boot_shelly_buffer_is_unavailable);
    RUN_TEST(test_true_then_false_before_processing_runs_rising_and_clear_passes_in_order);
    RUN_TEST(test_imu_true_then_false_before_processing_preserves_tamper_edge);
    RUN_TEST(test_gpio_pulse_between_alarm_passes_runs_rising_then_clear_in_order);
    RUN_TEST(test_gpio_edges_only_override_rules_with_matching_selector);
    RUN_TEST(test_gpio_edge_retries_after_coordinator_lock_timeout);
    RUN_TEST(test_failed_coordinator_pass_remains_pending_until_mutex_recovers);
    RUN_TEST(test_rising_edge_rtc_failure_rolls_back_and_retries_exactly_once);
    RUN_TEST(test_clear_edge_rtc_failure_rolls_back_and_retries_exactly_once);
    RUN_TEST(test_retained_triggered_state_is_cleared_by_first_definitive_false);
    RUN_TEST(test_begin_reanchors_retained_active_cooldown_without_boot_reminder);
    RUN_TEST(test_begin_fails_closed_when_boot_runtime_commit_fails);
    RUN_TEST(test_begin_fails_closed_when_boot_snapshot_read_is_incomplete);
    RUN_TEST(test_begin_maps_retained_runtime_by_rule_identity_after_reorder);
    RUN_TEST(test_begin_rejects_retained_runtime_after_torn_semantic_update);
    RUN_TEST(test_executor_install_reconciles_retained_global_shelly_or_once_per_device);
    RUN_TEST(test_unrelated_edit_and_reorder_preserve_retained_csi_state_by_id);
    RUN_TEST(test_explicit_csi_disable_resets_runtime_and_clears_aggregate);
    RUN_TEST(test_semantic_change_turns_off_old_active_shelly_binding);
    RUN_TEST(test_active_shelly_binding_diff_turns_removed_off_and_added_on);
    RUN_TEST(test_new_inactive_shelly_binding_is_explicitly_reconciled_off);
    RUN_TEST(test_transient_shelly_admission_failure_retries_without_new_alarm_edge);
    RUN_TEST(test_pending_shelly_outbox_prevents_second_rule_commit_from_overflowing);
    RUN_TEST(test_full_shelly_outbox_rejects_new_intent_and_rule_commit);
    RUN_TEST(test_terminal_missing_shelly_target_is_not_retried_forever);
    RUN_TEST(test_shelly_config_reconcile_applies_active_alarm_when_missing_peer_is_added);
    RUN_TEST(test_shared_shelly_binding_uses_global_or_when_one_rule_clears);
    RUN_TEST(test_removing_shared_binding_keeps_device_on_for_other_active_rule);
    RUN_TEST(test_pending_old_on_finishes_before_disable_reconciles_final_off);
    RUN_TEST(test_rtc_commit_failure_restores_rules_without_shelly_effects);
    RUN_TEST(test_rule_update_rejects_duplicate_or_partial_input_without_mutation);
    return UNITY_END();
}

#endif  // UNIT_TEST
