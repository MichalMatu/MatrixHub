#include <unity.h>

#include "../../lib/matrix_service/core/MatrixState.cpp"

void setUp(void) {}
void tearDown(void) {}

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

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_request_text_preserves_payload_up_to_command_buffer_size);
    RUN_TEST(test_request_effect_carries_engine_reactivity_and_background_cache);
    RUN_TEST(test_request_data_visualization_carries_config_and_replaces_effect_background);
    return UNITY_END();
}
