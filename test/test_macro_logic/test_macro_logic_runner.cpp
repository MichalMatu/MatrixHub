#include <unity.h>

void test_initialization();
void test_settings_persistence();
void test_engine_clear_state();
void test_execution_gaming_commands();
void test_execution_all_other_commands();
void test_use_case_gaming_afk();
void test_use_case_login_sequence();
void test_psram_exhaustion_safety();
void test_engine_execution_budget_limits_commands_and_runtime();

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_initialization);
    RUN_TEST(test_settings_persistence);
    RUN_TEST(test_execution_gaming_commands);
    RUN_TEST(test_execution_all_other_commands);
    RUN_TEST(test_use_case_gaming_afk);
    RUN_TEST(test_use_case_login_sequence);
    RUN_TEST(test_psram_exhaustion_safety);
    RUN_TEST(test_engine_execution_budget_limits_commands_and_runtime);
    RUN_TEST(test_engine_clear_state);
    return UNITY_END();
}
