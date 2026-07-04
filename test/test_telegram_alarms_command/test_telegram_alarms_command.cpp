/**
 * @file test_telegram_alarms_command.cpp
 * @brief Unit tests for Telegram /alarms command formatting.
 */

#include <unity.h>
#include <cstring>

#define NATIVE_BUILD 1

#include "../../src/alarms/AlarmRulesStore.h"
#include "../../src/notifications/telegram/commands/TelegramCommandTypes.h"
#include "../../src/notifications/telegram/commands/TelegramReplyBuilder.cpp"
#include "../../src/notifications/telegram/commands/AlarmsCommand.cpp"

using namespace TELEGRAM::Commands;

namespace {

ALARMS::AlarmRulesSnapshot s_rules{};
bool s_copyResult = true;

CommandContext createCtx() {
    CommandContext ctx = {};
    ctx.response[0] = '\0';
    ctx.responseLen = 0;
    ctx.shouldReply = false;
    return ctx;
}

ALARMS::AlarmRule makeRule(const char* id,
                           const char* name,
                           ALARMS::AlarmSource source,
                           ALARMS::AlarmOperator op,
                           float threshold,
                           ALARMS::AlarmSeverity severity) {
    ALARMS::AlarmRule rule;
    std::strncpy(rule.id, id, sizeof(rule.id) - 1);
    std::strncpy(rule.name, name, sizeof(rule.name) - 1);
    rule.enabled = true;
    rule.source = source;
    rule.op = op;
    rule.threshold = threshold;
    rule.severity = severity;
    return rule;
}

}  // namespace

namespace ALARMS::RULES_CONFIG {

bool copyTo(AlarmRulesSnapshot& out) {
    out = s_rules;
    return s_copyResult;
}

}  // namespace ALARMS::RULES_CONFIG

void setUp(void) {
    s_rules = ALARMS::AlarmRulesSnapshot{};
    s_copyResult = true;
}

void tearDown(void) {}

void test_alarms_command_reports_empty_rules() {
    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(handleAlarms(ctx));

    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_NOT_NULL(std::strstr(ctx.response, "Alarm Rules"));
    TEST_ASSERT_NOT_NULL(std::strstr(ctx.response, "No alarm rules configured."));
}

void test_alarms_command_formats_gpio_rule_with_channel_id() {
    s_rules.ruleCount = 1;
    s_rules.rules[0] = makeRule(
        "gpio-1",
        "Door input",
        ALARMS::AlarmSource::GpioDigital,
        ALARMS::AlarmOperator::Above,
        0.5f,
        ALARMS::AlarmSeverity::Warning);
    std::strncpy(s_rules.rules[0].gpioId, "gpio1", sizeof(s_rules.rules[0].gpioId) - 1);

    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(handleAlarms(ctx));

    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_NOT_NULL(std::strstr(ctx.response, "[ON] Door input"));
    TEST_ASSERT_NOT_NULL(std::strstr(ctx.response, "GPIO gpio1 > 0.5"));
    TEST_ASSERT_NULL(std::strstr(ctx.response, "Unknown"));
}

void test_alarms_command_formats_imu_tamper_label() {
    s_rules.ruleCount = 1;
    s_rules.rules[0] = makeRule(
        "imu-1",
        "Moved",
        ALARMS::AlarmSource::ImuTamper,
        ALARMS::AlarmOperator::Above,
        12.0f,
        ALARMS::AlarmSeverity::Critical);

    CommandContext ctx = createCtx();

    TEST_ASSERT_TRUE(handleAlarms(ctx));

    TEST_ASSERT_TRUE(ctx.shouldReply);
    TEST_ASSERT_NOT_NULL(std::strstr(ctx.response, "[ON] Moved"));
    TEST_ASSERT_NOT_NULL(std::strstr(ctx.response, "IMU Tamper > 12.0"));
    TEST_ASSERT_NULL(std::strstr(ctx.response, "Unknown"));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_alarms_command_reports_empty_rules);
    RUN_TEST(test_alarms_command_formats_gpio_rule_with_channel_id);
    RUN_TEST(test_alarms_command_formats_imu_tamper_label);
    return UNITY_END();
}
