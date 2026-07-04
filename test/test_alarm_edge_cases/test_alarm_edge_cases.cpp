/**
 * @file test_alarm_edge_cases.cpp
 * @brief Unit tests for alarm edge cases: Infinity, NaN sequences, Hot-Swap, Flapping
 */

#include <unity.h>
#include <cmath>
#include <cstring>
#include "../../src/alarms/engine/AlarmEvaluator.h"
#include "../../src/alarms/core/AlarmLogic.h"
#include "../../src/alarms/core/AlarmLogic.cpp" // Include impl for native testing

using namespace ALARMS;

// ============================================================================
// Helpers
// ============================================================================

AlarmRule createRule(AlarmSource source, AlarmOperator op, float threshold, uint16_t cooldown = 60) {
    AlarmRule rule;
    memset(&rule, 0, sizeof(rule));
    rule.enabled = true;
    rule.source = source;
    rule.op = op;
    rule.threshold = threshold;
    rule.cooldownSeconds = cooldown;
    // Default channel: LED
    rule.notifyChannels = NotifyChannel::Led;
    return rule;
}

AlarmInputData createData(float temp) {
    AlarmInputData data;
    data.temperature = temp;
    return data;
}

// Helper: Create EvaluationResult manually for Logic testing
EvaluationResult createResult(bool triggered, bool stateChanged, bool shouldNotify) {
    EvaluationResult res;
    res.triggered = triggered;
    res.stateChanged = stateChanged;
    res.shouldNotify = shouldNotify;
    res.rule = nullptr; 
    return res;
}

// ============================================================================
// Tests
// ============================================================================

void setUp(void) {}
void tearDown(void) {}

// ----------------------------------------------------------------------------
// 1. Infinity Handling
// ----------------------------------------------------------------------------

void test_infinity_positive() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Input: INFINITY
    EvaluationResult res = AlarmEvaluator::evaluate(rule, createData(INFINITY), state, 1000);
    
    TEST_ASSERT_FALSE(res.triggered);
    TEST_ASSERT_FALSE(res.shouldNotify);
    TEST_ASSERT_FALSE(state.previouslyTriggered);
}

void test_infinity_negative() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Below, 10.0f);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Input: -INFINITY
    EvaluationResult res = AlarmEvaluator::evaluate(rule, createData(-INFINITY), state, 1000);
    
    TEST_ASSERT_FALSE(res.triggered);
    TEST_ASSERT_FALSE(res.shouldNotify);
    TEST_ASSERT_FALSE(state.previouslyTriggered);
}

// ----------------------------------------------------------------------------
// 2. NaN Interruption (The WiFi Sensing Scenario)
// ----------------------------------------------------------------------------

void test_nan_interruption_maintains_state() {
    // Threshold > 30, Cooldown 60s
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // Step 1: Valid Trigger
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(40.0f), state, 1000);
    TEST_ASSERT_TRUE(res1.triggered);
    TEST_ASSERT_TRUE(res1.shouldNotify);
    TEST_ASSERT_EQUAL(1000, state.lastTriggeredMs);
    TEST_ASSERT_TRUE(state.previouslyTriggered);

    // Step 2: NaN Update (e.g., from WiFi Sensing path)
    // Should NOT clear the alarm state or reset cooldown
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(NAN), state, 2000);
    TEST_ASSERT_FALSE(res2.triggered);      // Evaluator returns false for triggered on NaN
    TEST_ASSERT_FALSE(res2.shouldNotify);   // No new notification
    TEST_ASSERT_FALSE(res2.stateChanged);   // IMPORTANT: NaN input should NOT be treated as state change (Clear)
                                            // Wait, let's verify Evaluator implementation...
                                            // The implementation returns EARLY on NaN. 
                                            // Does it update `previouslyTriggered`? No.
                                            // So it returns `result` which is zero-initialized. 
                                            // `result.stateChanged` will be false (default).
                                            // Correct.

    // Step 3: Valid Trigger Again (Still > 30)
    // Time passed: 3000ms (still under 60s cooldown)
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(35.0f), state, 3000);
    
    // Expect: Triggered, BUT Should Notify = FALSE (Cooldown)
    TEST_ASSERT_TRUE(res3.triggered);
    TEST_ASSERT_FALSE(res3.shouldNotify); // Cooldown active!
    // State should still be 'previouslyTriggered'
    TEST_ASSERT_TRUE(state.previouslyTriggered);
}

// ----------------------------------------------------------------------------
// 3. Hot-Swap Configuration
// ----------------------------------------------------------------------------

void test_channel_change_while_active() {
    // Setup Rule with LED only
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    rule.notifyChannels = NotifyChannel::Led;

    // Simulate Active State
    EvaluationResult res = createResult(true, true, true); // Triggered, Changed, Notify
    
    // Logic determines action
    AlarmAction action1 = AlarmLogic::determineAction(rule, res, false);
    TEST_ASSERT_TRUE(action1.sendNotify);
    // Logic shouldn't care about channels primarily, BUT
    // Evaluator doesn't know about channels. 
    // Coordinator distributes based on rule.notifyChannels.
    
    // Let's modify the rule configuration "in-flight"
    rule.notifyChannels = NotifyChannel::Led | NotifyChannel::Pushover;
    
    // Next evaluation (still triggered, cooldown not passed, no notify)
    EvaluationResult res2 = createResult(true, false, false); 
    AlarmAction action2 = AlarmLogic::determineAction(rule, res2, false);
    TEST_ASSERT_FALSE(action2.sendNotify);

    // Now, cooldown passes (simulated)
    EvaluationResult res3 = createResult(true, false, true); // Triggered, NoChange, Notify=True (cooldown done)
    AlarmAction action3 = AlarmLogic::determineAction(rule, res3, false);
    TEST_ASSERT_TRUE(action3.sendNotify);

    // Verification:
    // In Coordinator, `AlarmNotifier::notify` uses `rule.notifyChannels`.
    // Since we updated `rule` struct (which passes by reference/copy),
    // the Notifier WILL use the new bitmask.
    
    // Let's verify our bitmask helper:
    TEST_ASSERT_TRUE(hasChannel(rule.notifyChannels, NotifyChannel::Pushover));
}

// ----------------------------------------------------------------------------
// 4. Flapping (Quick Toggle)
// ----------------------------------------------------------------------------

void test_flapping_under_cooldown() {
    AlarmRule rule = createRule(AlarmSource::Temperature, AlarmOperator::Above, 30.0f, 60);
    AlarmRuntimeState state;
    memset(&state, 0, sizeof(state));

    // 1. Trig (Time=1000)
    EvaluationResult res1 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 1000);
    TEST_ASSERT_TRUE(res1.shouldNotify);
    TEST_ASSERT_EQUAL(1000, state.lastTriggeredMs);
    TEST_ASSERT_TRUE(state.previouslyTriggered);

    // 2. Clear (Time=2000) - Flapped down
    EvaluationResult res2 = AlarmEvaluator::evaluate(rule, createData(29.0f), state, 2000);
    TEST_ASSERT_FALSE(res2.triggered);
    TEST_ASSERT_TRUE(res2.stateChanged);
    TEST_ASSERT_FALSE(state.previouslyTriggered);
    // DOES IT RESET COOLDOWN?
    // In current implementation: yes, `lastTriggeredMs` is zeroed or ignored if not triggered?
    // Let's see Evaluator logic: 
    // "if (!triggered) { ... state.previouslyTriggered = false; return result; }"
    // It does NOT explicitly zero `lastTriggeredMs` unless we implement it.
    // BUT! Next time we trigger, we check `(now - lastTriggeredMs)`.
    
    // 3. Trig again (Time=3000) - Flapped up
    EvaluationResult res3 = AlarmEvaluator::evaluate(rule, createData(31.0f), state, 3000);
    TEST_ASSERT_TRUE(res3.triggered);
    
    // HERE IS THE QUEST: Should it notify?
    // `state.previouslyTriggered` was false.
    // Evaluator checks: `if (!state.previouslyTriggered) { shouldNotify = true; }`
    // YES! A new rising edge usually bypasses cooldown or resets it.
    TEST_ASSERT_TRUE(res3.shouldNotify);
    TEST_ASSERT_EQUAL(3000, state.lastTriggeredMs); // Should update timestamp
}


int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_infinity_positive);
    RUN_TEST(test_infinity_negative);
    RUN_TEST(test_nan_interruption_maintains_state);
    RUN_TEST(test_channel_change_while_active);
    RUN_TEST(test_flapping_under_cooldown);

    return UNITY_END();
}
