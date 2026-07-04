#include <unity.h>
#include <ArduinoJson.h>

#include "../../src/matrix/MatrixCustomIconStore.h"
#include "../../src/system/rtc/RtcConfig.h"

namespace RTC {
static ConfigStore mockStore;

const ConfigStore& getConfig() {
    return mockStore;
}

ConfigStore& getMutableConfig() {
    return mockStore;
}

void withConfig(const std::function<void(const ConfigStore&)>& reader) {
    reader(mockStore);
}

bool updateConfig(const std::function<void(ConfigStore&)>& updater) {
    updater(mockStore);
    return true;
}
}  // namespace RTC

namespace MATRIX {
static bool customIconHas[kMatrixCustomIconCount]{};
static uint32_t customIcons[kMatrixCustomIconCount][kMatrixCustomIconPixels]{};

bool hasCustomIcon(size_t index) {
    return index < kMatrixCustomIconCount && customIconHas[index];
}

bool copyCustomIcon(size_t index, uint32_t* outBuffer, size_t pixelCount) {
    if (index >= kMatrixCustomIconCount || !outBuffer || pixelCount != kMatrixCustomIconPixels ||
        !customIconHas[index]) {
        return false;
    }
    memcpy(outBuffer, customIcons[index], sizeof(customIcons[index]));
    return true;
}

bool setCustomIcon(size_t index, const uint32_t* bitmap, size_t pixelCount) {
    if (index >= kMatrixCustomIconCount || pixelCount != kMatrixCustomIconPixels) {
        return false;
    }
    if (!bitmap) {
        memset(customIcons[index], 0, sizeof(customIcons[index]));
        customIconHas[index] = false;
        return true;
    }
    for (size_t pixel = 0; pixel < kMatrixCustomIconPixels; ++pixel) {
        customIcons[index][pixel] = normalizeMatrixColor(bitmap[pixel]);
    }
    customIconHas[index] = true;
    return true;
}

bool clearCustomIcon(size_t index) {
    return setCustomIcon(index, nullptr);
}

void clearAllCustomIcons() {
    memset(customIcons, 0, sizeof(customIcons));
    memset(customIconHas, 0, sizeof(customIconHas));
}

bool customIconEquals(size_t index, const uint32_t* bitmap, size_t pixelCount) {
    if (index >= kMatrixCustomIconCount || pixelCount != kMatrixCustomIconPixels ||
        !customIconHas[index] || !bitmap) {
        return false;
    }
    for (size_t pixel = 0; pixel < kMatrixCustomIconPixels; ++pixel) {
        if (customIcons[index][pixel] != normalizeMatrixColor(bitmap[pixel])) {
            return false;
        }
    }
    return true;
}
}  // namespace MATRIX

#include "../../src/config/json/MatrixConfigJson.cpp"

void setUp(void) {
    RTC::mockStore = RTC::ConfigStore{};
    MATRIX::clearAllCustomIcons();
}

void tearDown(void) {}

void test_deserialize_matrix_normalizes_rgb888_colors() {
    JsonDocument doc;
    doc[CONFIG::Keys::kEffectColor] = 0x1ABCDEF;
    doc[CONFIG::Keys::kEffectColor2] = 0x1000000;
    doc[CONFIG::Keys::kEffectColor3] = 0xFFFFFFFF;
    doc[CONFIG::Keys::kMenuTextColor] = 0x1FFFFFF;

    RTC::MatrixData data{};
    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeMatrix(obj, data);

    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, data.effectColor);
    TEST_ASSERT_EQUAL_HEX32(0x000000, data.effectColor2);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, data.effectColor3);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, data.menu.textColor);
}

void test_deserialize_matrix_normalizes_native_3d_effect_fields() {
    JsonDocument doc;
    doc[CONFIG::Keys::kEffectEngine] = 1;
    doc[CONFIG::Keys::kEffectMode] = 69;
    doc[CONFIG::Keys::kEffectReactivityProvider] = 9;
    doc[CONFIG::Keys::kEffectReactivityGain] = 255;

    RTC::MatrixData data{};
    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeMatrix(obj, data);

    TEST_ASSERT_EQUAL_UINT8(1, data.effectEngine);
    TEST_ASSERT_EQUAL_UINT8(0, data.effectMode);
    TEST_ASSERT_EQUAL_UINT8(0, data.effectReactivityProvider);
    TEST_ASSERT_EQUAL_UINT8(200, data.effectReactivityGain);
}

void test_serialize_matrix_writes_effect_engine_and_reactivity_fields() {
    RTC::MatrixData data{};
    data.effectEngine = 1;
    data.effectMode = 3;
    data.effectReactivityProvider = 1;
    data.effectReactivityGain = 125;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    CONFIG::JSON::serializeMatrix(data, obj);

    TEST_ASSERT_EQUAL_UINT8(1, obj[CONFIG::Keys::kEffectEngine].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(3, obj[CONFIG::Keys::kEffectMode].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(1, obj[CONFIG::Keys::kEffectReactivityProvider].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(125, obj[CONFIG::Keys::kEffectReactivityGain].as<uint8_t>());
}

void test_deserialize_matrix_normalizes_data_visualization_fields() {
    JsonDocument doc;
    doc[CONFIG::Keys::kBackgroundMode] = 9;
    doc[CONFIG::Keys::kDataVisualizationEnabled] = true;
    doc[CONFIG::Keys::kDataVisualizationSource] = 9;
    doc[CONFIG::Keys::kDataVisualizationMetric] = 9;
    doc[CONFIG::Keys::kDataVisualizationMode] = 9;
    doc[CONFIG::Keys::kDataVisualizationMin] = 30.0f;
    doc[CONFIG::Keys::kDataVisualizationMax] = 20.0f;
    doc[CONFIG::Keys::kDataVisualizationColorMin] = 0x1ABCDEF;
    doc[CONFIG::Keys::kDataVisualizationColorMid] = 0x1000000;
    doc[CONFIG::Keys::kDataVisualizationColorMax] = 0xFFFFFFFF;
    doc[CONFIG::Keys::kDataVisualizationBrightnessMin] = 200;
    doc[CONFIG::Keys::kDataVisualizationBrightnessMax] = 10;
    doc[CONFIG::Keys::kDataVisualizationStaleBehavior] = 9;
    doc[CONFIG::Keys::kDataVisualizationDeviceId] = "AA:BB:CC:DD:EE:FF-extra";

    RTC::MatrixData data{};
    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeMatrix(obj, data);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::Effects), data.backgroundMode);
    TEST_ASSERT_FALSE(data.dataVisualizationEnabled);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataSource::Scd4x), data.dataVisualizationSource);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataMetric::Co2), data.dataVisualizationMetric);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Gauge), data.dataVisualizationMode);
    TEST_ASSERT_EQUAL_FLOAT(30.0f, data.dataVisualizationMin);
    TEST_ASSERT_EQUAL_FLOAT(31.0f, data.dataVisualizationMax);
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, data.dataVisualizationColorMin);
    TEST_ASSERT_EQUAL_HEX32(0x000000, data.dataVisualizationColorMid);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFF, data.dataVisualizationColorMax);
    TEST_ASSERT_EQUAL_UINT8(10, data.dataVisualizationBrightnessMin);
    TEST_ASSERT_EQUAL_UINT8(200, data.dataVisualizationBrightnessMax);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Dim),
                            data.dataVisualizationStaleBehavior);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", data.dataVisualizationDeviceId);
}

void test_deserialize_matrix_data_visualization_background_disables_effects() {
    JsonDocument doc;
    doc[CONFIG::Keys::kBackgroundMode] =
        static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    doc[CONFIG::Keys::kEffectEnabled] = true;
    doc[CONFIG::Keys::kDataVisualizationEnabled] = true;

    RTC::MatrixData data{};
    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeMatrix(obj, data);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization),
                            data.backgroundMode);
    TEST_ASSERT_FALSE(data.effectEnabled);
    TEST_ASSERT_TRUE(data.dataVisualizationEnabled);
}

void test_deserialize_matrix_effect_background_disables_data_visualization() {
    JsonDocument doc;
    doc[CONFIG::Keys::kBackgroundMode] = static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::Effects);
    doc[CONFIG::Keys::kEffectEnabled] = true;
    doc[CONFIG::Keys::kDataVisualizationEnabled] = true;

    RTC::MatrixData data{};
    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::deserializeMatrix(obj, data);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::Effects),
                            data.backgroundMode);
    TEST_ASSERT_TRUE(data.effectEnabled);
    TEST_ASSERT_FALSE(data.dataVisualizationEnabled);
}

void test_serialize_matrix_writes_data_visualization_fields() {
    RTC::MatrixData data{};
    data.backgroundMode = static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization);
    data.dataVisualizationEnabled = true;
    data.dataVisualizationSource = static_cast<uint8_t>(MATRIX::MatrixDataSource::BleThermometer);
    data.dataVisualizationMetric = static_cast<uint8_t>(MATRIX::MatrixDataMetric::Humidity);
    data.dataVisualizationMode = static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Trend);
    data.dataVisualizationMin = 10.0f;
    data.dataVisualizationMax = 90.0f;
    data.dataVisualizationColorMin = 0x102030;
    data.dataVisualizationColorMid = 0x405060;
    data.dataVisualizationColorMax = 0x708090;
    data.dataVisualizationBrightnessMin = 3;
    data.dataVisualizationBrightnessMax = 99;
    data.dataVisualizationSmoothing = 25;
    data.dataVisualizationStaleBehavior = static_cast<uint8_t>(MATRIX::MatrixDataStaleBehavior::Gray);
    MATRIX::copyMatrixDataDeviceId(
        data.dataVisualizationDeviceId,
        sizeof(data.dataVisualizationDeviceId),
        "11:22:33:44:55:66");

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    CONFIG::JSON::serializeMatrix(data, obj);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixBackgroundMode::DataVisualization),
                            obj[CONFIG::Keys::kBackgroundMode].as<uint8_t>());
    TEST_ASSERT_TRUE(obj[CONFIG::Keys::kDataVisualizationEnabled].as<bool>());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataSource::BleThermometer),
                            obj[CONFIG::Keys::kDataVisualizationSource].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataMetric::Humidity),
                            obj[CONFIG::Keys::kDataVisualizationMetric].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX::MatrixDataVizMode::Trend),
                            obj[CONFIG::Keys::kDataVisualizationMode].as<uint8_t>());
    TEST_ASSERT_EQUAL_FLOAT(10.0f, obj[CONFIG::Keys::kDataVisualizationMin].as<float>());
    TEST_ASSERT_EQUAL_FLOAT(90.0f, obj[CONFIG::Keys::kDataVisualizationMax].as<float>());
    TEST_ASSERT_EQUAL_HEX32(0x102030, obj[CONFIG::Keys::kDataVisualizationColorMin].as<uint32_t>());
    TEST_ASSERT_EQUAL_HEX32(0x405060, obj[CONFIG::Keys::kDataVisualizationColorMid].as<uint32_t>());
    TEST_ASSERT_EQUAL_HEX32(0x708090, obj[CONFIG::Keys::kDataVisualizationColorMax].as<uint32_t>());
    TEST_ASSERT_EQUAL_UINT8(3, obj[CONFIG::Keys::kDataVisualizationBrightnessMin].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(99, obj[CONFIG::Keys::kDataVisualizationBrightnessMax].as<uint8_t>());
    TEST_ASSERT_EQUAL_UINT8(25, obj[CONFIG::Keys::kDataVisualizationSmoothing].as<uint8_t>());
    TEST_ASSERT_EQUAL_STRING("11:22:33:44:55:66", obj[CONFIG::Keys::kDataVisualizationDeviceId].as<const char*>());
}

void test_load_matrix_psram_normalizes_custom_icon_pixels() {
    JsonDocument doc;
    JsonArray icons = doc[CONFIG::Keys::kCustomIcons].to<JsonArray>();
    JsonArray icon = icons.add<JsonArray>();
    for (size_t pixel = 0; pixel < MATRIX::kMatrixCustomIconPixels; ++pixel) {
        icon.add(pixel == 0 ? 0x1ABCDEF : pixel);
    }

    JsonObject obj = doc.as<JsonObject>();
    CONFIG::JSON::loadMatrixPsram(obj);

    uint32_t out[MATRIX::kMatrixCustomIconPixels]{};
    TEST_ASSERT_TRUE(MATRIX::copyCustomIcon(0, out));
    TEST_ASSERT_EQUAL_HEX32(0xABCDEF, out[0]);
    TEST_ASSERT_EQUAL_HEX32(1, out[1]);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_deserialize_matrix_normalizes_rgb888_colors);
    RUN_TEST(test_deserialize_matrix_normalizes_native_3d_effect_fields);
    RUN_TEST(test_serialize_matrix_writes_effect_engine_and_reactivity_fields);
    RUN_TEST(test_deserialize_matrix_normalizes_data_visualization_fields);
    RUN_TEST(test_deserialize_matrix_data_visualization_background_disables_effects);
    RUN_TEST(test_deserialize_matrix_effect_background_disables_data_visualization);
    RUN_TEST(test_serialize_matrix_writes_data_visualization_fields);
    RUN_TEST(test_load_matrix_psram_normalizes_custom_icon_pixels);
    return UNITY_END();
}
