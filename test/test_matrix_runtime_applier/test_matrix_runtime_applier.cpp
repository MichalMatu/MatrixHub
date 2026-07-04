#ifdef UNIT_TEST

#include <unity.h>

#include "../../src/matrix/MatrixRuntimeApplier.h"

namespace ALARMS {

void AlarmService::reapplyLatchedState() {}

bool AlarmService::isAlarmLatched() const {
    return false;
}

}  // namespace ALARMS

namespace MATRIX {

void MatrixMenuService::invalidateCache() {}

}  // namespace MATRIX

namespace MATRIX_MANAGER {

namespace {
uint32_t g_setLayerCalls = 0;
uint32_t g_clearLayerCalls = 0;
uint32_t g_invalidateCalls = 0;
Layer g_lastClearedLayer = Layer::BACKGROUND;
Layer g_lastSetLayer = Layer::BACKGROUND;
LayerContent g_lastSetContent;
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
    g_lastSetLayer = layer;
    g_lastSetContent = content;
    g_setLayerCalls++;
}

void MatrixManagerService::clearLayer(Layer layer) {
    g_lastClearedLayer = layer;
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

void MatrixManagerService::invalidateCache() {
    g_invalidateCalls++;
}

}  // namespace MATRIX_MANAGER

namespace {

MatrixService g_matrixService;

void resetState() {
    g_matrixService.clearCalls = 0;
    g_matrixService.clearBackgroundEffectCalls = 0;
    g_matrixService.clearBackgroundDataVisualizationCalls = 0;
    g_matrixService.showEffectCalls = 0;
    g_matrixService.showDataVisualizationCalls = 0;
    g_matrixService.showTextCalls = 0;
    g_matrixService.lastScrollSpeed = 0;
    MATRIX_MANAGER::g_setLayerCalls = 0;
    MATRIX_MANAGER::g_clearLayerCalls = 0;
    MATRIX_MANAGER::g_invalidateCalls = 0;
    MATRIX_MANAGER::g_lastSetContent = MATRIX_MANAGER::LayerContent{};
}

MATRIX::MatrixSettingsState makeState(bool effectEnabled) {
    MATRIX::MatrixSettingsState state{};
    state.config.brightness = 42;
    state.config.menu.scrollSpeed = 33;
    state.config.effectEnabled = effectEnabled;
    state.config.effectEngine = 1;
    state.config.effectMode = 3;
    state.config.effectSpeed = 900;
    state.config.effectColor = 0x112233;
    state.config.effectColor2 = 0x445566;
    state.config.effectColor3 = 0x778899;
    state.config.effectReactivityProvider = 1;
    state.config.effectReactivityGain = 125;
    return state;
}

}  // namespace

void setUp() {
    resetState();
}

void tearDown() {}

void test_apply_without_manager_clears_renderer_when_effects_disabled() {
    MATRIX::MatrixRuntimeApplier applier(&g_matrixService, nullptr, nullptr, nullptr);

    const MATRIX::MatrixSettingsState state = makeState(false);
    applier.apply(state);

    // Fallback mode without a layer manager still owns the renderer directly.
    TEST_ASSERT_EQUAL_UINT32(1, g_matrixService.clearCalls);
    TEST_ASSERT_EQUAL_UINT32(1, g_matrixService.clearBackgroundEffectCalls);
    TEST_ASSERT_EQUAL_UINT32(1, g_matrixService.clearBackgroundDataVisualizationCalls);
    TEST_ASSERT_EQUAL_UINT32(0, MATRIX_MANAGER::g_clearLayerCalls);
}

void test_apply_with_manager_clears_background_layer_without_direct_renderer_clear() {
    MATRIX_MANAGER::MatrixManagerService manager(&g_matrixService);
    MATRIX::MatrixRuntimeApplier applier(&g_matrixService, nullptr, &manager, nullptr);

    const MATRIX::MatrixSettingsState state = makeState(false);
    applier.apply(state);

    // Regression guard: with MatrixManager present we must not blank the whole
    // renderer, otherwise higher layers like MENU disappear until re-published.
    TEST_ASSERT_EQUAL_UINT32(0, g_matrixService.clearCalls);
    TEST_ASSERT_EQUAL_UINT32(1, g_matrixService.clearBackgroundEffectCalls);
    TEST_ASSERT_EQUAL_UINT32(1, g_matrixService.clearBackgroundDataVisualizationCalls);
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_clearLayerCalls);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(MATRIX_MANAGER::Layer::BACKGROUND),
        static_cast<uint8_t>(MATRIX_MANAGER::g_lastClearedLayer));
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_invalidateCalls);
}

void test_apply_with_manager_sets_background_layer_when_effects_enabled() {
    MATRIX_MANAGER::MatrixManagerService manager(&g_matrixService);
    MATRIX::MatrixRuntimeApplier applier(&g_matrixService, nullptr, &manager, nullptr);

    const MATRIX::MatrixSettingsState state = makeState(true);
    applier.apply(state);

    TEST_ASSERT_EQUAL_UINT32(0, g_matrixService.showEffectCalls);
    TEST_ASSERT_EQUAL_UINT32(0, g_matrixService.clearBackgroundDataVisualizationCalls);
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(MATRIX_MANAGER::Layer::BACKGROUND),
        static_cast<uint8_t>(MATRIX_MANAGER::g_lastSetLayer));
    TEST_ASSERT_EQUAL_UINT8(CommandType::SHOW_EFFECT, MATRIX_MANAGER::g_lastSetContent.type);
    TEST_ASSERT_EQUAL_UINT8(1, MATRIX_MANAGER::g_lastSetContent.effectEngine);
    TEST_ASSERT_EQUAL_UINT8(3, MATRIX_MANAGER::g_lastSetContent.effectMode);
    TEST_ASSERT_EQUAL_UINT16(900, MATRIX_MANAGER::g_lastSetContent.effectSpeed);
    TEST_ASSERT_EQUAL_HEX32(0x112233, MATRIX_MANAGER::g_lastSetContent.effectColor);
    TEST_ASSERT_EQUAL_HEX32(0x445566, MATRIX_MANAGER::g_lastSetContent.effectColor2);
    TEST_ASSERT_EQUAL_HEX32(0x778899, MATRIX_MANAGER::g_lastSetContent.effectColor3);
    TEST_ASSERT_EQUAL_UINT8(1, MATRIX_MANAGER::g_lastSetContent.effectReactivityProvider);
    TEST_ASSERT_EQUAL_UINT8(125, MATRIX_MANAGER::g_lastSetContent.effectReactivityGain);
}

void test_apply_with_manager_sets_data_visualization_background_layer() {
    MATRIX_MANAGER::MatrixManagerService manager(&g_matrixService);
    MATRIX::MatrixRuntimeApplier applier(&g_matrixService, nullptr, &manager, nullptr);

    MATRIX::MatrixSettingsState state = makeState(true);
    state.config.backgroundMode = static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    state.config.dataVisualizationEnabled = true;
    state.config.dataVisualizationSource = static_cast<uint8_t>(MATRIX::MatrixDataSource::BleThermometer);
    state.config.dataVisualizationMetric = static_cast<uint8_t>(MATRIX::MatrixDataMetric::Temperature);
    state.config.dataVisualizationMode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap);
    state.config.dataVisualizationMin = 18.0f;
    state.config.dataVisualizationMax = 28.0f;
    state.config.dataVisualizationColorMin = 0x001122;
    state.config.dataVisualizationColorMid = 0x334455;
    state.config.dataVisualizationColorMax = 0x667788;
    state.config.dataVisualizationBrightnessMin = 9;
    state.config.dataVisualizationBrightnessMax = 99;
    state.config.dataVisualizationSmoothing = 30;
    state.config.dataVisualizationStaleBehavior =
        static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Gray);
    MATRIX::copyMatrixDataDeviceId(
        state.config.dataVisualizationDeviceId,
        sizeof(state.config.dataVisualizationDeviceId),
        "AA:BB:CC:DD:EE:FF");

    applier.apply(state);

    TEST_ASSERT_EQUAL_UINT32(0, g_matrixService.showEffectCalls);
    TEST_ASSERT_EQUAL_UINT32(0, g_matrixService.clearBackgroundEffectCalls);
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(MATRIX_MANAGER::Layer::BACKGROUND),
        static_cast<uint8_t>(MATRIX_MANAGER::g_lastSetLayer));
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(CommandType::SHOW_DATA_VISUALIZATION),
        static_cast<uint8_t>(MATRIX_MANAGER::g_lastSetContent.type));
    const auto& config = MATRIX_MANAGER::g_lastSetContent.dataVisualizationConfig;
    TEST_ASSERT_TRUE(config.enabled);
    TEST_ASSERT_EQUAL_UINT8(
        static_cast<uint8_t>(MATRIX::MatrixDataSource::BleThermometer),
        config.source);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap), config.mode);
    TEST_ASSERT_EQUAL_FLOAT(18.0f, config.minValue);
    TEST_ASSERT_EQUAL_FLOAT(28.0f, config.maxValue);
    TEST_ASSERT_EQUAL_HEX32(0x001122, config.colorMin);
    TEST_ASSERT_EQUAL_HEX32(0x334455, config.colorMid);
    TEST_ASSERT_EQUAL_HEX32(0x667788, config.colorMax);
    TEST_ASSERT_EQUAL_UINT8(9, config.brightnessMin);
    TEST_ASSERT_EQUAL_UINT8(99, config.brightnessMax);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", config.deviceId);
}

void test_apply_normalizes_csi_data_visualization_range() {
    MATRIX_MANAGER::MatrixManagerService manager(&g_matrixService);
    MATRIX::MatrixRuntimeApplier applier(&g_matrixService, nullptr, &manager, nullptr);

    MATRIX::MatrixSettingsState state = makeState(true);
    state.config.backgroundMode = static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    state.config.dataVisualizationEnabled = true;
    state.config.dataVisualizationSource = static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi);
    state.config.dataVisualizationMetric = static_cast<uint8_t>(MATRIX::MatrixDataMetric::CsiMotion);
    state.config.dataVisualizationMode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Heatmap);
    state.config.dataVisualizationMin = -100.0f;
    state.config.dataVisualizationMax = 100.0f;

    applier.apply(state);

    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    const auto& config = MATRIX_MANAGER::g_lastSetContent.dataVisualizationConfig;
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataSource::WifiCsi), config.source);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, config.minValue);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, config.maxValue);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_apply_without_manager_clears_renderer_when_effects_disabled);
    RUN_TEST(test_apply_with_manager_clears_background_layer_without_direct_renderer_clear);
    RUN_TEST(test_apply_with_manager_sets_background_layer_when_effects_enabled);
    RUN_TEST(test_apply_with_manager_sets_data_visualization_background_layer);
    RUN_TEST(test_apply_normalizes_csi_data_visualization_range);
    return UNITY_END();
}

#endif
