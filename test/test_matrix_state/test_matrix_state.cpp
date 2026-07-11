#include <unity.h>

#include "../../lib/matrix_service/core/MatrixState.cpp"

void setUp(void) {
    TEST_STUBS::FREERTOS::resetSemaphoreTakeStats();
}
void tearDown(void) {}

void assertBrightnessCommand(MatrixState& state, uint8_t expected) {
    MatrixCommand command;
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_BRIGHTNESS),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_UINT8(expected, command.value8);
}

void test_request_text_preserves_payload_up_to_command_buffer_size() {
    MatrixState state;
    MatrixCommand command;

    char longText[kMatrixTextCapacity];
    memset(longText, 'A', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';

    state.requestText(longText, 0x123456, 2500);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_TEXT), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_STRING(longText, command.text);
    TEST_ASSERT_EQUAL_HEX32(0x123456, command.color);
    TEST_ASSERT_EQUAL_UINT32(2500, command.durationMs);
}

void test_request_effect_carries_engine_reactivity_and_background_cache() {
    MatrixState state;
    MatrixCommand command;

    state.requestEffect(3, 850, 0x010203, 0x040506, 0x070809, 0, 1, 1, 125);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_EFFECT), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_UINT8(3, command.value8);
    TEST_ASSERT_EQUAL_UINT8(1, command.effectEngine);
    TEST_ASSERT_EQUAL_UINT32(850, command.effectSpeedMs);
    TEST_ASSERT_EQUAL_HEX32(0x010203, command.value32);
    TEST_ASSERT_EQUAL_HEX32(0x040506, command.value32_2);
    TEST_ASSERT_EQUAL_HEX32(0x070809, command.value32_3);
    TEST_ASSERT_EQUAL_UINT8(1, command.effectReactivityProvider);
    TEST_ASSERT_EQUAL_UINT8(125, command.effectReactivityGain);

    const auto bg = state.getBackgroundEffect();
    TEST_ASSERT_TRUE(bg.active);
    TEST_ASSERT_EQUAL_UINT8(3, bg.mode);
    TEST_ASSERT_EQUAL_UINT8(1, bg.engine);
    TEST_ASSERT_EQUAL_UINT8(1, bg.reactivityProvider);
    TEST_ASSERT_EQUAL_UINT8(125, bg.reactivityGain);
}

void test_request_data_visualization_carries_config_and_replaces_effect_background() {
    MatrixState state;
    MatrixCommand command;

    state.requestEffect(3, 850, 0x010203, 0x040506, 0x070809, 0, 1, 1, 125);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_TRUE(state.getBackgroundEffect().active);

    MATRIX::MatrixDataVisualizationConfig config;
    config.enabled = true;
    config.source = static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi);
    config.metric = static_cast<uint8_t>(MATRIX::MatrixDataMetric::CsiMotion);
    config.mode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap);
    config.minValue = 0.0f;
    config.maxValue = 100.0f;
    MATRIX::copyMatrixDataDeviceId(config.deviceId, sizeof(config.deviceId), "AA:BB:CC:DD:EE:FF");

    state.requestDataVisualization(config, 0);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_DATA_VISUALIZATION), static_cast<int>(command.type));
    TEST_ASSERT_TRUE(command.dataVisualizationConfig.enabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi),
                            command.dataVisualizationConfig.source);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", command.dataVisualizationConfig.deviceId);

    TEST_ASSERT_FALSE(state.getBackgroundEffect().active);
    const auto bgViz = state.getBackgroundDataVisualization();
    TEST_ASSERT_TRUE(bgViz.active);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap),
                            bgViz.config.mode);
}

void test_latest_content_wins_after_pending_hardware_update() {
    MatrixState state;
    MatrixCommand command;

    state.setBrightness(42);
    state.setRotation(2);
    state.requestEffect(3, 850, 0x010203, 0x040506, 0x070809, 0, 1, 1, 125);
    state.requestSolid(0xAA0000);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_BRIGHTNESS),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_UINT8(42, command.value8);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_ROTATION),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_UINT8(2, command.value8);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_SOLID),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_HEX32(0xAA0000, command.color);

    TEST_ASSERT_FALSE(state.poll(command));
}

void test_thermal_cap_uses_min_of_latest_user_target_and_limit() {
    MatrixState state;

    state.setBrightness(80);
    assertBrightnessCommand(state, 80);
    state.setThermalBrightnessLimit(16);
    assertBrightnessCommand(state, 16);
    state.setThermalBrightnessLimit(2);
    assertBrightnessCommand(state, 2);
    state.setThermalBrightnessLimit(255);
    assertBrightnessCommand(state, 80);
    TEST_ASSERT_FALSE(state.hasPendingCommands());
}

void test_user_brightness_changed_while_muted_restores_latest_target() {
    MatrixState state;

    state.setBrightness(80);
    assertBrightnessCommand(state, 80);
    state.setThermalBrightnessLimit(0);
    assertBrightnessCommand(state, 0);

    state.setBrightness(40);
    TEST_ASSERT_FALSE(state.hasPendingCommands());
    state.setThermalBrightnessLimit(16);
    assertBrightnessCommand(state, 16);
    state.setThermalBrightnessLimit(255);
    assertBrightnessCommand(state, 40);
}

void test_repeated_cap_sequence_never_inverts_or_sticks() {
    MatrixState state;

    state.setBrightness(64);
    assertBrightnessCommand(state, 64);
    state.setThermalBrightnessLimit(16);
    assertBrightnessCommand(state, 16);
    state.setThermalBrightnessLimit(2);
    assertBrightnessCommand(state, 2);
    state.setThermalBrightnessLimit(16);
    assertBrightnessCommand(state, 16);
    state.setThermalBrightnessLimit(255);
    assertBrightnessCommand(state, 64);
}

void test_user_target_below_cap_is_never_brightened() {
    MatrixState state;

    state.setBrightness(8);
    assertBrightnessCommand(state, 8);
    state.setThermalBrightnessLimit(16);
    TEST_ASSERT_FALSE(state.hasPendingCommands());
    state.setThermalBrightnessLimit(2);
    assertBrightnessCommand(state, 2);
    state.setThermalBrightnessLimit(255);
    assertBrightnessCommand(state, 8);
}

void test_duplicate_and_coalesced_caps_preserve_latest_content_order() {
    MatrixState state;
    MatrixCommand command;

    state.setBrightness(80);
    state.setThermalBrightnessLimit(16);
    state.requestEffect(3, 850, 1, 2, 3, 0);
    state.requestSolid(0xAA0000);
    state.setThermalBrightnessLimit(16);

    assertBrightnessCommand(state, 16);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_SOLID),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_HEX32(0xAA0000, command.color);
    TEST_ASSERT_FALSE(state.poll(command));

    state.setThermalBrightnessLimit(2);
    state.setThermalBrightnessLimit(16);
    TEST_ASSERT_FALSE(state.hasPendingCommands());
}

void test_new_text_supersedes_pending_clear_without_replaying_clear() {
    MatrixState state;
    MatrixCommand command;

    state.requestClear(true);
    state.requestText("READY", 0x00AA00, 1500);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_TEXT),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_STRING("READY", command.text);
    TEST_ASSERT_FALSE(state.poll(command));
}

void test_new_clear_supersedes_pending_icon() {
    MatrixState state;
    MatrixCommand command;

    state.requestIcon(IconType::ALARM_WARNING, 3000);
    state.requestClear(false);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::CLEAR),
                      static_cast<int>(command.type));
    TEST_ASSERT_FALSE(command.stopBackground);
    TEST_ASSERT_FALSE(state.poll(command));
}

void test_each_visible_content_type_can_be_the_latest_command() {
    MatrixState state;
    MatrixCommand command;
    MATRIX::MatrixDataVisualizationConfig config{};
    config.enabled = true;

    state.requestText("old", 0x010203, 10);
    state.requestIcon(IconType::ALARM_INFO, 20);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_ICON), static_cast<int>(command.type));

    state.requestIcon(IconType::ALARM_WARNING, 20);
    state.requestText("new", 0x040506, 30);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_TEXT), static_cast<int>(command.type));

    state.requestText("old", 0x010203, 10);
    state.requestSolid(0xAABBCC);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_SOLID), static_cast<int>(command.type));

    state.requestSolid(0xAABBCC);
    state.requestEffect(4, 500, 1, 2, 3, 1000);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_EFFECT), static_cast<int>(command.type));

    state.requestEffect(4, 500, 1, 2, 3, 1000);
    state.requestDataVisualization(config, 1000);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_DATA_VISUALIZATION),
                      static_cast<int>(command.type));

    state.requestDataVisualization(config, 1000);
    state.requestClear(false);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::CLEAR), static_cast<int>(command.type));
    TEST_ASSERT_FALSE(state.poll(command));
}

void test_clearing_effect_cache_cancels_only_pending_persistent_effect() {
    MatrixState state;
    MatrixCommand command;

    state.setBrightness(42);
    state.setRotation(2);
    state.requestEffect(3, 850, 1, 2, 3, 0);
    state.clearBackgroundEffect();

    TEST_ASSERT_FALSE(state.getBackgroundEffect().active);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_BRIGHTNESS),
                      static_cast<int>(command.type));
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_ROTATION),
                      static_cast<int>(command.type));
    TEST_ASSERT_FALSE(state.poll(command));

    state.requestEffect(4, 500, 4, 5, 6, 1200);
    state.clearBackgroundEffect();

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_EFFECT), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_UINT32(1200, command.durationMs);
}

void test_clearing_visualization_cache_cancels_only_pending_persistent_visualization() {
    MatrixState state;
    MatrixCommand command;
    MATRIX::MatrixDataVisualizationConfig config{};
    config.enabled = true;

    state.setBrightness(41);
    state.setRotation(3);
    state.requestDataVisualization(config, 0);
    state.clearBackgroundDataVisualization();

    TEST_ASSERT_FALSE(state.getBackgroundDataVisualization().active);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_BRIGHTNESS),
                      static_cast<int>(command.type));
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SET_ROTATION),
                      static_cast<int>(command.type));
    TEST_ASSERT_FALSE(state.poll(command));

    state.requestDataVisualization(config, 1200);
    state.clearBackgroundDataVisualization();

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_DATA_VISUALIZATION),
                      static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_UINT32(1200, command.durationMs);
}

void test_clear_stop_background_flag_controls_cached_background_state() {
    MatrixState state;
    MatrixCommand command;

    state.requestEffect(3, 850, 1, 2, 3, 0);
    TEST_ASSERT_TRUE(state.poll(command));
    state.requestClear(false);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_TRUE(state.getBackgroundEffect().active);

    MATRIX::MatrixDataVisualizationConfig config{};
    config.enabled = true;
    state.requestDataVisualization(config, 0);
    TEST_ASSERT_TRUE(state.poll(command));
    state.requestClear(false);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_TRUE(state.getBackgroundDataVisualization().active);

    state.requestClear(true);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_FALSE(state.getBackgroundEffect().active);
    TEST_ASSERT_FALSE(state.getBackgroundDataVisualization().active);
}

void test_matrix_state_uses_non_expiring_mailbox_lock() {
    MatrixState state;

    state.requestText("LOCKED", 0xFFFFFF, 100);

    TEST_ASSERT_EQUAL_UINT32(portMAX_DELAY,
                             TEST_STUBS::FREERTOS::lastSemaphoreTakeTimeout);
    TEST_ASSERT_EQUAL_UINT32(1, TEST_STUBS::FREERTOS::semaphoreTakeCount);
}

void test_failed_writer_lock_does_not_replace_pending_content() {
    MatrixState state;
    MatrixCommand command;

    state.requestText("SAFE", 0x123456, 250);
    TEST_STUBS::FREERTOS::failNextSemaphoreTake();
    state.requestEffect(4, 500, 1, 2, 3, 0);

    TEST_ASSERT_EQUAL_UINT32(portMAX_DELAY,
                             TEST_STUBS::FREERTOS::lastSemaphoreTakeTimeout);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_TEXT), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_STRING("SAFE", command.text);
    TEST_ASSERT_FALSE(state.getBackgroundEffect().active);
}

void test_failed_poll_lock_leaves_output_and_pending_content_untouched() {
    MatrixState state;
    MatrixCommand command;
    command.type = CommandType::SHOW_SOLID;
    command.color = 0xABCDEF;

    state.requestText("PENDING", 0x654321, 500);
    TEST_STUBS::FREERTOS::failNextSemaphoreTake();

    TEST_ASSERT_FALSE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_SOLID), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, command.color);

    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_TEXT), static_cast<int>(command.type));
    TEST_ASSERT_EQUAL_STRING("PENDING", command.text);
}

void test_clearing_other_background_cache_preserves_latest_persistent_content() {
    MatrixState state;
    MatrixCommand command;
    MATRIX::MatrixDataVisualizationConfig config{};
    config.enabled = true;

    state.requestDataVisualization(config, 0);
    state.clearBackgroundEffect();
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_DATA_VISUALIZATION),
                      static_cast<int>(command.type));

    state.requestEffect(3, 850, 1, 2, 3, 0);
    state.clearBackgroundDataVisualization();
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_EFFECT), static_cast<int>(command.type));
}

void test_failed_background_clear_lock_preserves_cache_and_pending_content() {
    MatrixState state;
    MatrixCommand command;

    state.requestEffect(3, 850, 1, 2, 3, 0);
    TEST_STUBS::FREERTOS::failNextSemaphoreTake();
    state.clearBackgroundEffect();

    TEST_ASSERT_TRUE(state.getBackgroundEffect().active);
    TEST_ASSERT_TRUE(state.poll(command));
    TEST_ASSERT_EQUAL(static_cast<int>(CommandType::SHOW_EFFECT), static_cast<int>(command.type));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_request_text_preserves_payload_up_to_command_buffer_size);
    RUN_TEST(test_request_effect_carries_engine_reactivity_and_background_cache);
    RUN_TEST(test_request_data_visualization_carries_config_and_replaces_effect_background);
    RUN_TEST(test_latest_content_wins_after_pending_hardware_update);
    RUN_TEST(test_thermal_cap_uses_min_of_latest_user_target_and_limit);
    RUN_TEST(test_user_brightness_changed_while_muted_restores_latest_target);
    RUN_TEST(test_repeated_cap_sequence_never_inverts_or_sticks);
    RUN_TEST(test_user_target_below_cap_is_never_brightened);
    RUN_TEST(test_duplicate_and_coalesced_caps_preserve_latest_content_order);
    RUN_TEST(test_new_text_supersedes_pending_clear_without_replaying_clear);
    RUN_TEST(test_new_clear_supersedes_pending_icon);
    RUN_TEST(test_each_visible_content_type_can_be_the_latest_command);
    RUN_TEST(test_clearing_effect_cache_cancels_only_pending_persistent_effect);
    RUN_TEST(test_clearing_visualization_cache_cancels_only_pending_persistent_visualization);
    RUN_TEST(test_clear_stop_background_flag_controls_cached_background_state);
    RUN_TEST(test_matrix_state_uses_non_expiring_mailbox_lock);
    RUN_TEST(test_failed_writer_lock_does_not_replace_pending_content);
    RUN_TEST(test_failed_poll_lock_leaves_output_and_pending_content_untouched);
    RUN_TEST(test_clearing_other_background_cache_preserves_latest_persistent_content);
    RUN_TEST(test_failed_background_clear_lock_preserves_cache_and_pending_content);
    return UNITY_END();
}
