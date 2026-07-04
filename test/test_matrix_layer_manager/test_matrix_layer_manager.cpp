#include <unity.h>

#include "../../src/system/matrix_manager/MatrixLayerManager.h"
#include "../../src/system/matrix_manager/MatrixLayerManager.cpp"

using namespace MATRIX_MANAGER;

void setUp(void) {}
void tearDown(void) {}

static LayerContent makeTextLayer(const char* text, uint32_t color) {
    LayerContent content;
    content.type = CommandType::SHOW_TEXT;
    content.color = color;
    strlcpy(content.text, text, sizeof(content.text));
    return content;
}

void test_top_layer_returns_cached_hash() {
    MatrixLayerManager manager;

    manager.setLayer(Layer::IDLE, makeTextLayer("idle", 0x112233));

    LayerContent top;
    Layer topLayer = Layer::BACKGROUND;
    uint32_t topHash = 0;
    TEST_ASSERT_TRUE(manager.getTopLayer(top, topLayer, topHash));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Layer::IDLE), static_cast<uint8_t>(topLayer));
    TEST_ASSERT_EQUAL_STRING("idle", top.text);
    TEST_ASSERT_NOT_EQUAL_UINT32(0, topHash);

    LayerContent idle;
    uint32_t idleHash = 0;
    TEST_ASSERT_TRUE(manager.getLayerContent(Layer::IDLE, idle, idleHash));
    TEST_ASSERT_EQUAL_UINT32(topHash, idleHash);
}

void test_hash_changes_when_layer_content_changes() {
    MatrixLayerManager manager;

    manager.setLayer(Layer::ALARM, makeTextLayer("first", 0xAA0000));
    LayerContent first;
    Layer layer = Layer::BACKGROUND;
    uint32_t firstHash = 0;
    TEST_ASSERT_TRUE(manager.getTopLayer(first, layer, firstHash));

    manager.setLayer(Layer::ALARM, makeTextLayer("second", 0xAA0000));
    LayerContent second;
    uint32_t secondHash = 0;
    TEST_ASSERT_TRUE(manager.getTopLayer(second, layer, secondHash));

    TEST_ASSERT_EQUAL_STRING("second", second.text);
    TEST_ASSERT_NOT_EQUAL_UINT32(firstHash, secondHash);
}

void test_hash_changes_when_effect_reactivity_changes() {
    MatrixLayerManager manager;

    LayerContent first;
    first.type = CommandType::SHOW_EFFECT;
    first.effectEngine = 1;
    first.effectMode = 2;
    first.effectReactivityProvider = 0;
    first.effectReactivityGain = 80;
    manager.setLayer(Layer::BACKGROUND, first);

    LayerContent top;
    Layer layer = Layer::BACKGROUND;
    uint32_t firstHash = 0;
    TEST_ASSERT_TRUE(manager.getTopLayer(top, layer, firstHash));

    LayerContent second = first;
    second.effectReactivityProvider = 1;
    second.effectReactivityGain = 125;
    manager.setLayer(Layer::BACKGROUND, second);

    uint32_t secondHash = 0;
    TEST_ASSERT_TRUE(manager.getTopLayer(top, layer, secondHash));

    TEST_ASSERT_NOT_EQUAL_UINT32(firstHash, secondHash);
}

void test_clear_removes_layer_and_hash() {
    MatrixLayerManager manager;

    manager.setLayer(Layer::SYSTEM_MODAL, makeTextLayer("modal", 0xFFFFFF));
    manager.clearLayer(Layer::SYSTEM_MODAL);

    LayerContent content;
    uint32_t hash = 123;
    TEST_ASSERT_FALSE(manager.getLayerContent(Layer::SYSTEM_MODAL, content, hash));
    TEST_ASSERT_EQUAL_UINT32(0, hash);

    Layer topLayer = Layer::BACKGROUND;
    TEST_ASSERT_FALSE(manager.getTopLayer(content, topLayer, hash));
    TEST_ASSERT_EQUAL_UINT32(0, hash);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_top_layer_returns_cached_hash);
    RUN_TEST(test_hash_changes_when_layer_content_changes);
    RUN_TEST(test_hash_changes_when_effect_reactivity_changes);
    RUN_TEST(test_clear_removes_layer_and_hash);
    return UNITY_END();
}
