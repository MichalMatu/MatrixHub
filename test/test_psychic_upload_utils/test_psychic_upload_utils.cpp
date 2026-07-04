#include <unity.h>

#include "../../lib/PsychicHttp/src/PsychicUploadUtils.cpp"

void test_parse_content_disposition_reads_full_quoted_filename() {
    const ContentDisposition cd = PsychicUploadUtils::parseContentDisposition(
        "form-data; name=\"firmware\"; filename=\"update.bin\"");

    TEST_ASSERT_EQUAL(FORM_DATA, cd.disposition);
    TEST_ASSERT_EQUAL_STRING("firmware", cd.name.c_str());
    TEST_ASSERT_EQUAL_STRING("update.bin", cd.filename.c_str());
}

void test_parse_content_disposition_ignores_missing_filename() {
    const ContentDisposition cd = PsychicUploadUtils::parseContentDisposition(
        "form-data; name=\"firmware\"");

    TEST_ASSERT_EQUAL(FORM_DATA, cd.disposition);
    TEST_ASSERT_EQUAL_STRING("firmware", cd.name.c_str());
    TEST_ASSERT_TRUE(cd.filename.isEmpty());
}

void test_resolve_filename_uses_query_param_value_not_name() {
    const PsychicWebParameter param("_filename", "actual.bin");

    const String filename = PsychicUploadUtils::resolveFilename("", &param, "/upload/fallback.bin");

    TEST_ASSERT_EQUAL_STRING("actual.bin", filename.c_str());
}

void test_resolve_filename_prefers_header_over_query_and_uri() {
    const PsychicWebParameter param("_filename", "query.bin");

    const String filename = PsychicUploadUtils::resolveFilename(
        "attachment; filename=\"header.bin\"",
        &param,
        "/upload/path.bin");

    TEST_ASSERT_EQUAL_STRING("header.bin", filename.c_str());
}

void test_resolve_filename_strips_query_string_from_uri_fallback() {
    const String filename = PsychicUploadUtils::resolveFilename(
        "",
        nullptr,
        "/upload/images/archive.tar.gz?token=123");

    TEST_ASSERT_EQUAL_STRING("archive.tar.gz", filename.c_str());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_parse_content_disposition_reads_full_quoted_filename);
    RUN_TEST(test_parse_content_disposition_ignores_missing_filename);
    RUN_TEST(test_resolve_filename_uses_query_param_value_not_name);
    RUN_TEST(test_resolve_filename_prefers_header_over_query_and_uri);
    RUN_TEST(test_resolve_filename_strips_query_string_from_uri_fallback);
    return UNITY_END();
}
