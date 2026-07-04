/**
 * @file test_alarm_logic.cpp
 * @brief Unit tests for AlarmLogic pure business logic
 */

#include <unity.h>
#include <cstring>
#include "../../src/alarms/core/AlarmLogic.h"
#include "../../src/alarms/core/AlarmLogic.cpp" // Include impl for native testing

using namespace ALARMS;

// ============================================================================
// Helpers
// ============================================================================

AlarmRule createRule(const char* name, bool shelly = false, bool led = false) {
    AlarmRule rule;
    memset(&rule, 0, sizeof(rule));
    strcpy(rule.name, name);
    rule.enabled = true;
    rule.severity = AlarmSeverity::Warning;
    
    // Setup channels
    if (shelly) {
        // Mock shelly device
        rule.addShellyDevice("SHSW-25#123456#1");
    }
    
    // Clear channels first
    rule.notifyChannels = NotifyChannel::None;
    
    // Add requested
    if (led) {
        rule.notifyChannels = static_cast<NotifyChannel>(
            static_cast<uint8_t>(rule.notifyChannels) | static_cast<uint8_t>(NotifyChannel::Led)
        );
    }
    
    return rule;
}

EvaluationResult createResult(bool triggered, bool stateChanged, bool shouldNotify) {
    EvaluationResult res;
    res.triggered = triggered;
    res.stateChanged = stateChanged;
    res.shouldNotify = shouldNotify;
    res.rule = nullptr; // Not used by logic directly
    return res;
}

// ============================================================================
// Tests
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

void test_notification_logic() {
    AlarmRule rule = createRule("Test");
    
    // Case 1: Triggered and should notify
    EvaluationResult res1 = createResult(true, true, true);
    AlarmAction action1 = AlarmLogic::determineAction(rule, res1, false);
    TEST_ASSERT_TRUE(action1.sendNotify);
    TEST_ASSERT_FALSE(action1.sendClear);

    // Case 2: Triggered but cooldown (shouldNotify=false)
    EvaluationResult res2 = createResult(true, false, false);
    AlarmAction action2 = AlarmLogic::determineAction(rule, res2, false);
    TEST_ASSERT_FALSE(action2.sendNotify);
}

void test_cleared_notification_logic() {
    AlarmRule rule = createRule("Test");
    
    // Case: State changed to NOT triggered (Cleared)
    EvaluationResult res = createResult(false, true, false);
    AlarmAction action = AlarmLogic::determineAction(rule, res, false);
    
    TEST_ASSERT_TRUE(action.sendClear);
    TEST_ASSERT_FALSE(action.sendNotify);
}

void test_shelly_logic_normal_transition() {
    AlarmRule rule = createRule("ShellyRule", true);
    
    // Triggered (ON)
    EvaluationResult res1 = createResult(true, true, true);
    AlarmAction action1 = AlarmLogic::determineAction(rule, res1, false);
    TEST_ASSERT_TRUE(action1.triggerShelly);
    TEST_ASSERT_TRUE(action1.shellyState); // ON
    
    // Cleared (OFF)
    EvaluationResult res2 = createResult(false, true, false);
    AlarmAction action2 = AlarmLogic::determineAction(rule, res2, false);
    TEST_ASSERT_TRUE(action2.triggerShelly);
    TEST_ASSERT_FALSE(action2.shellyState); // OFF
}

void test_shelly_logic_no_change() {
    AlarmRule rule = createRule("ShellyRule", true);
    
    // Still triggered, no state change
    EvaluationResult res = createResult(true, false, true); // maybe separate notification needed
    AlarmAction action = AlarmLogic::determineAction(rule, res, false);
    
    TEST_ASSERT_FALSE(action.triggerShelly);
    // Notification might still proceed if cooldown elapsed (res.shouldNotify=true)
    TEST_ASSERT_TRUE(action.sendNotify);
}

void test_shelly_logic_force_sync_on_init() {
    AlarmRule rule = createRule("ShellyRule", true);
    
    // Not triggered, no state change, BUT wasUninitialized=true
    // Should verify Shelly is OFF
    EvaluationResult res = createResult(false, false, false);
    AlarmAction action = AlarmLogic::determineAction(rule, res, true); // wasUninitialized=true
    
    TEST_ASSERT_TRUE(action.triggerShelly);
    TEST_ASSERT_FALSE(action.shellyState); // Ensure OFF
}

void test_metric_aggregation_logic() {
    AlarmAggregateState agg = {false, AlarmSeverity::Info};
    
    // Rule 1: Warning, Active, LED Enabled
    AlarmRule r1 = createRule("R1", false, true);
    r1.severity = AlarmSeverity::Warning;
    AlarmRuntimeState s1;
    s1.previouslyTriggered = true;
    
    AlarmLogic::updateAggregate(r1, s1, agg);
    TEST_ASSERT_TRUE(agg.active);
    TEST_ASSERT_EQUAL(AlarmSeverity::Warning, agg.maxSeverity);
    
    // Rule 2: Critical, Active, LED Enabled
    AlarmRule r2 = createRule("R2", false, true);
    r2.severity = AlarmSeverity::Critical;
    AlarmRuntimeState s2;
    s2.previouslyTriggered = true;
    
    AlarmLogic::updateAggregate(r2, s2, agg);
    TEST_ASSERT_TRUE(agg.active);
    TEST_ASSERT_EQUAL(AlarmSeverity::Critical, agg.maxSeverity);
    
    // Rule 3: Info, Active, LED DISABLED
    AlarmRule r3 = createRule("R3", false, false); // No LED
    r3.severity = AlarmSeverity::Critical;
    AlarmRuntimeState s3;
    s3.previouslyTriggered = true;
    
    // Should NOT affect aggregation (assume we start fresh or keep previous)
    // Let's reset agg to test isolation
    agg = {false, AlarmSeverity::Info};
    AlarmLogic::updateAggregate(r3, s3, agg);
    TEST_ASSERT_FALSE(agg.active);
}

void test_metric_aggregation_keeps_first_name_for_same_severity() {
    AlarmAggregateState agg = {false, AlarmSeverity::Info};

    AlarmRule first = createRule("FirstWarning", false, true);
    first.severity = AlarmSeverity::Warning;
    AlarmRuntimeState firstState;
    firstState.previouslyTriggered = true;

    AlarmRule second = createRule("SecondWarning", false, true);
    second.severity = AlarmSeverity::Warning;
    AlarmRuntimeState secondState;
    secondState.previouslyTriggered = true;

    AlarmLogic::updateAggregate(first, firstState, agg);
    AlarmLogic::updateAggregate(second, secondState, agg);

    TEST_ASSERT_TRUE(agg.active);
    TEST_ASSERT_EQUAL(AlarmSeverity::Warning, agg.maxSeverity);
    TEST_ASSERT_EQUAL_STRING("FirstWarning", agg.alarmName);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_notification_logic);
    RUN_TEST(test_cleared_notification_logic);
    RUN_TEST(test_shelly_logic_normal_transition);
    RUN_TEST(test_shelly_logic_no_change);
    RUN_TEST(test_shelly_logic_force_sync_on_init);
    RUN_TEST(test_metric_aggregation_logic);
    RUN_TEST(test_metric_aggregation_keeps_first_name_for_same_severity);
    return UNITY_END();
}
