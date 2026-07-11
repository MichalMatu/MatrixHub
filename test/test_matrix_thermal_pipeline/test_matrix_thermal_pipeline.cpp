#include <unity.h>

// Keep this target on the production command path. The only fake below is the
// existing LedMatrix transport stub used by the renderer tests; command
// coalescing, service ordering, renderer state and timeout behavior are real.
#include "../../lib/matrix_service/effects/MatrixFxEngine3D.cpp"
#include "../../lib/matrix_service/visualization/MatrixDataVisualizationEngine.cpp"
#include "../../lib/matrix_service/core/MatrixState.cpp"

// MatrixRenderer.cpp and MatrixService.cpp both use a translation-unit-local
// TAG. Give each direct inclusion a unique identifier in this combined target.
#define TAG MATRIX_THERMAL_PIPELINE_RENDERER_TAG
#include "../../lib/matrix_service/renderer/MatrixRenderer.cpp"
#undef TAG

#define TAG MATRIX_THERMAL_PIPELINE_SERVICE_TAG
#include "../../lib/matrix_service/MatrixService.cpp"
#undef TAG

void IconDrawer::draw(LedMatrix* matrix, IconType icon, const uint32_t* customBitmap) {
    (void)icon;
    if (matrix) {
        matrix->drawBitmap(customBitmap);
    }
}

namespace {

LedMatrix& matrix() {
    TEST_ASSERT_NOT_NULL(LedMatrix::lastInstance);
    return *LedMatrix::lastInstance;
}

void initialize(MatrixService& service) {
    service.init(5);
    matrix().resetCounters();
}

}  // namespace

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 0;
    TEST_STUBS::FREERTOS::resetSemaphoreTakeStats();
    LedMatrix::lastInstance = nullptr;
}

void tearDown(void) {}

void test_brightness_precedes_rotation_and_content_and_keeps_muted_frame_untouched_while_pending() {
    MatrixService service;
    initialize(service);

    service.showEffect(11, 1000, 0x112233, 0x223344, 0x334455);
    service.loop();
    TEST_ASSERT_TRUE(service.isActive());
    matrix().resetCounters();

    // Deliberately enqueue these in reverse priority order. MatrixState must
    // still publish brightness, then rotation, then the latest visible content.
    service.showSolidColor(0x00AA00);
    service.setRotation(2);
    service.setBrightness(0);
    TEST_ASSERT_TRUE(service.isActive());

    service.loop();

    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastBrightness);
    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastRotation);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().restoreOutputCalls);
    TEST_ASSERT_TRUE(service.isActive());

    service.loop();

    TEST_ASSERT_EQUAL_UINT8(2, matrix().lastRotation);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().serviceCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().restoreOutputCalls);
    TEST_ASSERT_TRUE(service.isActive());

    service.loop();

    // The logical replacement is accepted only after both hardware updates.
    // The real LedMatrix driver keeps output physically muted at brightness 0;
    // its latched-black behavior is covered by test_led_matrix_driver.
    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_HEX32(0x00AA00, matrix().lastFillColor);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().stopCalls);
    TEST_ASSERT_FALSE(service.isActive());
}

void test_expired_timeout_has_no_side_effect_while_hardware_and_content_mailbox_drains() {
    MatrixService service;
    initialize(service);

    TEST_STUBS::ARDUINO::millisValue = 100;
    service.showIcon(IconType::ALARM_INFO, 50);
    service.loop();
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    matrix().resetCounters();

    // The old icon is expired now, but a newer visible command is queued behind
    // brightness and rotation. Neither the timeout clear nor the deferred icon
    // rotation repaint may run while that mailbox is non-empty.
    TEST_STUBS::ARDUINO::millisValue = 1000;
    service.showSolidColor(0xABCDEF);
    service.setRotation(1);
    service.setBrightness(0);

    service.loop();
    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastBrightness);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().restoreOutputCalls);
    TEST_ASSERT_TRUE(service.isActive());

    service.loop();
    TEST_ASSERT_EQUAL_UINT8(1, matrix().lastRotation);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().restoreOutputCalls);
    TEST_ASSERT_TRUE(service.isActive());

    service.loop();

    // SHOW_SOLID cancels the old timeout. Exactly one draw belongs to the new
    // content; an eager timeout path would have added an earlier black clear.
    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, matrix().lastFillColor);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
    TEST_ASSERT_FALSE(service.isActive());
}

void test_is_active_stays_true_until_static_mailbox_is_fully_drained() {
    MatrixService service;
    initialize(service);

    service.showSolidColor(0x123456);
    service.loop();
    TEST_ASSERT_FALSE(service.isActive());
    matrix().resetCounters();

    service.showIcon(IconType::ALARM_WARNING, 0);
    service.setRotation(3);
    service.setBrightness(0);

    TEST_ASSERT_TRUE(service.isActive());
    service.loop();
    TEST_ASSERT_TRUE(service.isActive());
    service.loop();
    TEST_ASSERT_TRUE(service.isActive());

    service.loop();

    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastBrightness);
    TEST_ASSERT_EQUAL_UINT8(3, matrix().lastRotation);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
    TEST_ASSERT_FALSE(service.isActive());
}

void test_restore_from_zero_does_not_flash_pre_mute_frame_before_new_content() {
    MatrixService service;
    initialize(service);

    service.showSolidColor(0x111111);
    service.setBrightness(80);
    service.loop();
    service.loop();
    matrix().resetCounters();

    service.setThermalBrightnessLimit(0);
    service.loop();
    TEST_ASSERT_EQUAL_UINT8(0, matrix().lastBrightness);
    matrix().resetCounters();

    service.showSolidColor(0x22AA44);
    service.setRotation(1);
    service.setThermalBrightnessLimit(255);

    service.loop();
    TEST_ASSERT_EQUAL_UINT8(80, matrix().lastBrightness);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().restoreOutputCalls);

    service.loop();
    TEST_ASSERT_EQUAL_UINT8(1, matrix().lastRotation);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().restoreOutputCalls);

    service.loop();
    TEST_ASSERT_EQUAL_UINT32(1, matrix().fillCalls);
    TEST_ASSERT_EQUAL_HEX32(0x22AA44, matrix().lastFillColor);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
    // MatrixRenderer checks the deferred restore after static content; the real
    // driver made that check a no-op when showSolid() cleared _restorePending.
    TEST_ASSERT_EQUAL_UINT32(1, matrix().restoreOutputCalls);
}

void test_terminal_blackout_resets_service_icon_cache_for_same_icon_republish() {
    MatrixService service;
    initialize(service);

    TEST_STUBS::ARDUINO::millisValue = 100;
    service.showIcon(IconType::ALARM_CRITICAL, 100);
    service.loop();
    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);

    service.blackoutForShutdown();
    matrix().resetCounters();

    // A terminal blackout also cancels the old timeout. Advancing past it must
    // not emit a stale clear before MatrixTask is conceptually started again.
    TEST_STUBS::ARDUINO::millisValue = 1000;
    service.loop();
    TEST_ASSERT_EQUAL_UINT32(0, matrix().fillCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(0, matrix().showCalls);
    matrix().resetCounters();

    // Re-publishing the same IconType must reach MatrixRenderer. Without
    // clearing MatrixService::_activeIcon during terminal blackout, the service
    // would incorrectly deduplicate this command and leave the panel black.
    service.showIcon(IconType::ALARM_CRITICAL, 0);
    service.loop();

    TEST_ASSERT_EQUAL_UINT32(1, matrix().drawBitmapCalls);
    TEST_ASSERT_EQUAL_UINT32(1, matrix().showCalls);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_brightness_precedes_rotation_and_content_and_keeps_muted_frame_untouched_while_pending);
    RUN_TEST(test_expired_timeout_has_no_side_effect_while_hardware_and_content_mailbox_drains);
    RUN_TEST(test_is_active_stays_true_until_static_mailbox_is_fully_drained);
    RUN_TEST(test_restore_from_zero_does_not_flash_pre_mute_frame_before_new_content);
    RUN_TEST(test_terminal_blackout_resets_service_icon_cache_for_same_icon_republish);
    return UNITY_END();
}
