#ifdef UNIT_TEST

#include <unity.h>

#include "../../src/config/UIColors.h"
#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/system/matrix_manager/MatrixManagerService.h"

namespace RTC {

ConfigStore mockStore{};

const ConfigStore& getConfig() {
    return mockStore;
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(mockStore);
}

}  // namespace RTC

namespace MATRIX_MANAGER {

namespace {

Layer g_lastLayer = Layer::BACKGROUND;
LayerContent g_lastContent;
uint32_t g_setLayerCalls = 0;
uint32_t g_clearLayerCalls = 0;

}  // namespace

MatrixLayerManager::MatrixLayerManager() {}

void MatrixLayerManager::setLayer(Layer layer, const LayerContent& content) {
    (void)layer;
    (void)content;
}

void MatrixLayerManager::clearLayer(Layer layer) {
    (void)layer;
}

bool MatrixLayerManager::getTopLayer(LayerContent& out, Layer& outLayer) const {
    (void)out;
    (void)outLayer;
    return false;
}

bool MatrixLayerManager::isLayerActive(Layer layer) const {
    (void)layer;
    return false;
}

bool MatrixLayerManager::getLayerContent(Layer layer, LayerContent& out) const {
    (void)layer;
    (void)out;
    return false;
}

MatrixNotificationQueue::MatrixNotificationQueue() {}

bool MatrixNotificationQueue::push(const Notification& n) {
    (void)n;
    return true;
}

bool MatrixNotificationQueue::peek(Notification& out) const {
    (void)out;
    return false;
}

void MatrixNotificationQueue::pop() {}
void MatrixNotificationQueue::clear() {}
bool MatrixNotificationQueue::empty() const { return true; }
uint8_t MatrixNotificationQueue::count() const { return 0; }

MatrixManagerService::MatrixManagerService(MatrixService* matrixService)
    : _matrixService(matrixService) {}

MatrixManagerService::~MatrixManagerService() = default;

void MatrixManagerService::setLayer(Layer layer, const LayerContent& content) {
    g_lastLayer = layer;
    g_lastContent = content;
    g_setLayerCalls++;
}

void MatrixManagerService::clearLayer(Layer layer) {
    g_lastLayer = layer;
    g_clearLayerCalls++;
}

bool MatrixManagerService::isLayerActive(Layer layer) const {
    (void)layer;
    return false;
}

void MatrixManagerService::queueNotification(const char* text, uint32_t color, uint32_t displayMs) {
    (void)text;
    (void)color;
    (void)displayMs;
}

uint8_t MatrixManagerService::pendingNotifications() const {
    return 0;
}

void MatrixManagerService::update() {}
void MatrixManagerService::invalidateCache() {}

}  // namespace MATRIX_MANAGER

#include "../../src/alarms/core/controller/display/AlarmDisplayEngine.cpp"
#include "../../src/alarms/core/controller/AlarmMatrixController.cpp"

namespace {

void resetControllerState() {
    memset(&RTC::mockStore, 0, sizeof(RTC::mockStore));
    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::SCROLL_TEXT;
    MATRIX_MANAGER::g_lastLayer = MATRIX_MANAGER::Layer::BACKGROUND;
    MATRIX_MANAGER::g_lastContent = MATRIX_MANAGER::LayerContent{};
    MATRIX_MANAGER::g_setLayerCalls = 0;
    MATRIX_MANAGER::g_clearLayerCalls = 0;
}

void assertLastLayerIsAlarm() {
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(MATRIX_MANAGER::Layer::ALARM),
        static_cast<uint8_t>(MATRIX_MANAGER::g_lastLayer));
}

void assertLastIcon(IconType expectedIcon) {
    assertLastLayerIsAlarm();
    TEST_ASSERT_EQUAL_UINT8(CommandType::SHOW_ICON, MATRIX_MANAGER::g_lastContent.type);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(expectedIcon),
        static_cast<uint8_t>(MATRIX_MANAGER::g_lastContent.icon));
}

void assertLastSolid(uint32_t expectedColor) {
    assertLastLayerIsAlarm();
    TEST_ASSERT_EQUAL_UINT8(CommandType::SHOW_SOLID, MATRIX_MANAGER::g_lastContent.type);
    TEST_ASSERT_EQUAL_UINT32(expectedColor, MATRIX_MANAGER::g_lastContent.color);
}

void assertLastScrollText(const char* expectedText, uint32_t expectedColor) {
    assertLastLayerIsAlarm();
    TEST_ASSERT_EQUAL_UINT8(CommandType::SHOW_TEXT, MATRIX_MANAGER::g_lastContent.type);
    TEST_ASSERT_EQUAL_STRING(expectedText, MATRIX_MANAGER::g_lastContent.text);
    TEST_ASSERT_EQUAL_UINT32(expectedColor, MATRIX_MANAGER::g_lastContent.color);
}

}  // namespace

void setUp(void) {
    resetControllerState();
}

void tearDown(void) {}

void test_icon_mode_maps_all_severities_to_icons() {
    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::ICON;

    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Info, "Info");
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    assertLastIcon(IconType::ALARM_INFO);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Warning, "Warn");
    TEST_ASSERT_EQUAL_UINT32(2, MATRIX_MANAGER::g_setLayerCalls);
    assertLastIcon(IconType::ALARM_WARNING);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Critical, "Crit");
    TEST_ASSERT_EQUAL_UINT32(3, MATRIX_MANAGER::g_setLayerCalls);
    assertLastIcon(IconType::ALARM_CRITICAL);
}

void test_solid_mode_maps_all_severities_to_colors() {
    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::SOLID_COLOR;

    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Info, "Info");
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    assertLastSolid(UI::COLOR::ALARM_INFO);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Warning, "Warn");
    TEST_ASSERT_EQUAL_UINT32(2, MATRIX_MANAGER::g_setLayerCalls);
    assertLastSolid(UI::COLOR::ALARM_WARNING);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Critical, "Crit");
    TEST_ASSERT_EQUAL_UINT32(3, MATRIX_MANAGER::g_setLayerCalls);
    assertLastSolid(UI::COLOR::ALARM_CRITICAL);
}

void test_scroll_text_mode_maps_text_and_colors() {
    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::SCROLL_TEXT;

    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Info, "Info alarm");
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    assertLastScrollText("Info alarm", UI::COLOR::ALARM_INFO);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Warning, "Warn alarm");
    TEST_ASSERT_EQUAL_UINT32(2, MATRIX_MANAGER::g_setLayerCalls);
    assertLastScrollText("Warn alarm", UI::COLOR::ALARM_WARNING);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Critical, "Crit alarm");
    TEST_ASSERT_EQUAL_UINT32(3, MATRIX_MANAGER::g_setLayerCalls);
    assertLastScrollText("Crit alarm", UI::COLOR::ALARM_CRITICAL);
}

void test_scroll_text_fallback_icon_respects_warning_severity() {
    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Warning, nullptr);

    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    assertLastIcon(IconType::ALARM_WARNING);
}

void test_scroll_text_fallback_icon_respects_info_severity_for_empty_name() {
    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    controller.updateDisplay(true, ALARMS::AlarmSeverity::Info, "");

    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    assertLastIcon(IconType::ALARM_INFO);
}

void test_update_clears_alarm_layer_when_alarm_deactivates() {
    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::ICON;

    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    ALARMS::AlarmAggregateState activeState{};
    activeState.active = true;
    activeState.maxSeverity = ALARMS::AlarmSeverity::Warning;
    ALARMS::safeCopyAlarmName(activeState.alarmName, "Pump dry");

    TEST_ASSERT_TRUE(controller.update(activeState));
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);

    ALARMS::AlarmAggregateState inactiveState{};
    TEST_ASSERT_TRUE(controller.update(inactiveState));
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_clearLayerCalls);
    assertLastLayerIsAlarm();
}

void test_reapply_latched_state_uses_updated_display_mode() {
    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::ICON;

    MATRIX_MANAGER::MatrixManagerService manager(nullptr);
    ALARMS::AlarmMatrixController controller;
    controller.setMatrixManager(&manager);

    ALARMS::AlarmAggregateState activeState{};
    activeState.active = true;
    activeState.maxSeverity = ALARMS::AlarmSeverity::Warning;
    ALARMS::safeCopyAlarmName(activeState.alarmName, "Water low");

    TEST_ASSERT_TRUE(controller.update(activeState));
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    assertLastIcon(IconType::ALARM_WARNING);

    RTC::mockStore.matrix.alarmMode = RTC::MatrixAlarmMode::SOLID_COLOR;
    controller.reapplyLatchedState();

    TEST_ASSERT_EQUAL_UINT32(2, MATRIX_MANAGER::g_setLayerCalls);
    assertLastSolid(UI::COLOR::ALARM_WARNING);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_icon_mode_maps_all_severities_to_icons);
    RUN_TEST(test_solid_mode_maps_all_severities_to_colors);
    RUN_TEST(test_scroll_text_mode_maps_text_and_colors);
    RUN_TEST(test_scroll_text_fallback_icon_respects_warning_severity);
    RUN_TEST(test_scroll_text_fallback_icon_respects_info_severity_for_empty_name);
    RUN_TEST(test_update_clears_alarm_layer_when_alarm_deactivates);
    RUN_TEST(test_reapply_latched_state_uses_updated_display_mode);
    return UNITY_END();
}

#endif
