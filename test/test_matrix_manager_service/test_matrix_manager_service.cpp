#include <unity.h>

#include "../../src/system/logging/Logging.h"

namespace LOG {

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

}  // namespace LOG

#include "../../src/system/matrix_manager/MatrixLayerManager.cpp"
#include "../../src/system/matrix_manager/MatrixNotificationQueue.cpp"
#include "../../src/system/matrix_manager/MatrixManagerService.cpp"

using namespace MATRIX_MANAGER;

namespace {

LayerContent makeSolid(uint32_t color) {
    LayerContent content;
    content.type = CommandType::SHOW_SOLID;
    content.color = color;
    return content;
}

LayerContent makeText(const char* text, uint32_t color, uint32_t durationMs) {
    LayerContent content;
    content.type = CommandType::SHOW_TEXT;
    strlcpy(content.text, text, sizeof(content.text));
    content.color = color;
    content.durationMs = durationMs;
    return content;
}

LayerContent makeEffect() {
    LayerContent content;
    content.type = CommandType::SHOW_EFFECT;
    content.effectMode = 3;
    content.effectSpeed = 850;
    content.effectColor = 0x010203;
    return content;
}

}  // namespace

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 100;
    TEST_STUBS::FREERTOS::resetSemaphoreTakeStats();
}

void tearDown(void) {}

void test_system_modal_timeout_restores_latched_alarm_layer() {
    MatrixService renderer;
    MatrixManagerService manager(&renderer);

    manager.setLayer(Layer::ALARM, makeSolid(0xAA0000));
    manager.update();
    TEST_ASSERT_EQUAL_UINT32(1, renderer.showSolidCalls);
    TEST_ASSERT_EQUAL_HEX32(0xAA0000, renderer.lastSolidColor);

    manager.setLayer(Layer::SYSTEM_MODAL, makeText("RESET?", 0xFFA000, 1000));
    TEST_STUBS::ARDUINO::millisValue = 200;
    manager.update();
    TEST_ASSERT_EQUAL_UINT32(1, renderer.showTextCalls);
    TEST_ASSERT_EQUAL_STRING("RESET?", renderer.lastText);

    TEST_STUBS::ARDUINO::millisValue = 1300;
    manager.update();
    TEST_ASSERT_EQUAL_UINT32(2, renderer.showSolidCalls);
    TEST_ASSERT_EQUAL_HEX32(0xAA0000, renderer.lastSolidColor);
    TEST_ASSERT_FALSE(manager.isLayerActive(Layer::SYSTEM_MODAL));
    TEST_ASSERT_TRUE(manager.isLayerActive(Layer::ALARM));
}

void test_invalidation_after_last_layer_removal_clears_renderer() {
    MatrixService renderer;
    MatrixManagerService manager(&renderer);

    manager.setLayer(Layer::BACKGROUND, makeEffect());
    manager.update();
    TEST_ASSERT_EQUAL_UINT32(1, renderer.showEffectCalls);

    manager.clearLayer(Layer::BACKGROUND);
    manager.invalidateCache();
    manager.update();

    TEST_ASSERT_EQUAL_UINT32(1, renderer.clearCalls);
    TEST_ASSERT_FALSE(renderer.lastClearStopBackground);
}

void test_invalidation_republishes_unchanged_active_layer() {
    MatrixService renderer;
    MatrixManagerService manager(&renderer);

    manager.setLayer(Layer::ALARM, makeSolid(0xBB0000));
    manager.update();
    manager.invalidateCache();
    manager.update();

    TEST_ASSERT_EQUAL_UINT32(2, renderer.showSolidCalls);
    TEST_ASSERT_EQUAL_HEX32(0xBB0000, renderer.lastSolidColor);
}

void test_reset_modal_stays_above_menu_until_cancel_then_restores_alarm() {
    MatrixService renderer;
    MatrixManagerService manager(&renderer);

    manager.setLayer(Layer::ALARM, makeSolid(0xCC0000));
    manager.update();

    TEST_STUBS::ARDUINO::millisValue = 200;
    manager.setLayer(Layer::MENU, makeText("MENU", 0xFFFFFF, 0));
    manager.update();
    TEST_ASSERT_EQUAL_STRING("MENU", renderer.lastText);

    // MonitoringInitializer closes MENU before publishing reset feedback.
    manager.clearLayer(Layer::MENU);
    manager.setLayer(Layer::RESET_MODAL, makeText("RESET?", 0xFFA000, 3000));
    TEST_STUBS::ARDUINO::millisValue = 300;
    manager.update();
    TEST_ASSERT_EQUAL_STRING("RESET?", renderer.lastText);

    manager.setLayer(Layer::RESET_MODAL, makeText("RELEASE +2x", 0xFF0000, 0));
    TEST_STUBS::ARDUINO::millisValue = 400;
    manager.update();
    TEST_ASSERT_EQUAL_STRING("RELEASE +2x", renderer.lastText);

    // A stale menu publish cannot obscure the critical reset prompt.
    manager.setLayer(Layer::MENU, makeText("STALE MENU", 0xFFFFFF, 0));
    TEST_STUBS::ARDUINO::millisValue = 6400;
    manager.update();
    TEST_ASSERT_TRUE(manager.isLayerActive(Layer::RESET_MODAL));
    TEST_ASSERT_EQUAL_STRING("RELEASE +2x", renderer.lastText);

    manager.setLayer(Layer::RESET_MODAL, makeText("CANCEL", 0x00C800, 1500));
    manager.clearLayer(Layer::MENU);
    manager.update();
    TEST_ASSERT_EQUAL_STRING("CANCEL", renderer.lastText);

    TEST_STUBS::ARDUINO::millisValue = 8000;
    manager.update();
    TEST_ASSERT_FALSE(manager.isLayerActive(Layer::RESET_MODAL));
    TEST_ASSERT_EQUAL_HEX32(0xCC0000, renderer.lastSolidColor);
    TEST_ASSERT_EQUAL_UINT32(2, renderer.showSolidCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_system_modal_timeout_restores_latched_alarm_layer);
    RUN_TEST(test_invalidation_after_last_layer_removal_clears_renderer);
    RUN_TEST(test_invalidation_republishes_unchanged_active_layer);
    RUN_TEST(test_reset_modal_stays_above_menu_until_cancel_then_restores_alarm);
    return UNITY_END();
}
