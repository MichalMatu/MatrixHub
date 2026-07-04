#include <unity.h>

#include "../../src/api/common/RequestQueryUtils.h"

void test_isTruthyParam_accepts_supported_truthy_spellings() {
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("1")));
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("true")));
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("TRUE")));
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("yes")));
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("YES")));
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("2")));
    TEST_ASSERT_TRUE(API::RequestQuery::isTruthyParam(String("-7")));
}

void test_isTruthyParam_rejects_empty_and_common_falsey_values() {
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("")));
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("0")));
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("00")));
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("false")));
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("FALSE")));
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("no")));
    TEST_ASSERT_FALSE(API::RequestQuery::isTruthyParam(String("garbage")));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_isTruthyParam_accepts_supported_truthy_spellings);
    RUN_TEST(test_isTruthyParam_rejects_empty_and_common_falsey_values);
    return UNITY_END();
}
