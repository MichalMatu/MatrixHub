#include <unity.h>

#include "../../src/system/matrix_manager/MatrixManagerTypes.h"

#include <cstring>

void setUp(void) {}
void tearDown(void) {}

void test_matrix_text_buffers_share_same_capacity() {
    MatrixCommand command;
    MATRIX_MANAGER::LayerContent layer;
    MATRIX_MANAGER::Notification notification;

    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(kMatrixTextCapacity),
        static_cast<uint32_t>(sizeof(command.text)));
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(kMatrixTextCapacity),
        static_cast<uint32_t>(sizeof(layer.text)));
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(kMatrixTextCapacity),
        static_cast<uint32_t>(sizeof(notification.text)));
}

void test_matrix_text_survives_notification_and_layer_handoff_without_early_truncation() {
    char longText[kMatrixTextCapacity];
    memset(longText, 'B', sizeof(longText) - 1);
    longText[sizeof(longText) - 1] = '\0';

    MATRIX_MANAGER::Notification notification;
    notification.setText(longText);
    TEST_ASSERT_EQUAL_STRING(longText, notification.text);

    MATRIX_MANAGER::LayerContent layer;
    strlcpy(layer.text, notification.text, sizeof(layer.text));
    TEST_ASSERT_EQUAL_STRING(longText, layer.text);

    MatrixCommand command;
    strlcpy(command.text, layer.text, sizeof(command.text));
    TEST_ASSERT_EQUAL_STRING(longText, command.text);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_matrix_text_buffers_share_same_capacity);
    RUN_TEST(test_matrix_text_survives_notification_and_layer_handoff_without_early_truncation);
    return UNITY_END();
}
