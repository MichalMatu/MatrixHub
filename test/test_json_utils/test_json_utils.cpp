#include <unity.h>
#include <string>
#include <vector>
#include <Arduino.h>

#include "../../test/stubs/PsychicJson.h"
#include "../../test/stubs/PsychicRequest.h"
#include "../../src/system/memory/PsramAllocator.h"
#include <utils/ResponseUtils.h>

// ============================================================================
// Mocks Setup
// ============================================================================

std::string g_responseBuffer;
std::string g_responseType;
std::string g_responseHeaderField;
std::string g_responseHeaderValue;

extern "C" {
    // Defines for esp_http_server mocks
    #include "esp_http_server.h"
    
    esp_err_t httpd_resp_set_type(httpd_req_t *r, const char *type) {
        g_responseType = (type ? type : "");
        return ESP_OK;
    }

    esp_err_t httpd_resp_set_hdr(httpd_req_t *r, const char *field, const char *value) {
        g_responseHeaderField = (field ? field : "");
        g_responseHeaderValue = (value ? value : "");
        return ESP_OK;
    }
    
    esp_err_t httpd_resp_send_chunk(httpd_req_t *r, const char *buf, ssize_t len) {
        if (len == 0) return ESP_OK; // Flush/Finish
        if (buf == NULL) return ESP_OK; // Also Flush
        g_responseBuffer.append(buf, len);
        return ESP_OK;
    }
}

// Include source files directly to link against mocks
#include "../../src/system/utils/json/JsonStreamReader.cpp"
#include "../../src/system/utils/json/JsonResponseWriter.cpp"

// ============================================================================
// Reader Tests
// ============================================================================

void setUp(void) {
    g_responseBuffer.clear();
    g_responseType.clear();
    g_responseHeaderField.clear();
    g_responseHeaderValue.clear();
    TEST_ASSERT_TRUE(Utils::JsonResponseWriter::begin());
}

void tearDown(void) {}

void test_reader_literals() {
    NetworkClient client;
    client.setMockData("  true false null 123 ");
    Utils::JsonStreamReader r(client);
    
    bool b = false;
    TEST_ASSERT_TRUE(r.readBool(b));
    TEST_ASSERT_TRUE(b);
    
    TEST_ASSERT_TRUE(r.readBool(b));
    TEST_ASSERT_FALSE(b);
    
    // null is not a bool, skipValue handles it
    // Let's reset stream to test null skip
    client.setMockData("null 999");
    Utils::JsonStreamReader r2(client);
    TEST_ASSERT_TRUE(r2.skipValue());
    
    int64_t i;
    TEST_ASSERT_TRUE(r2.readInt64(i));
    TEST_ASSERT_EQUAL_INT64(999, i);
}

void test_reader_string() {
    NetworkClient client;
    // Note: in C++ string literal, backslash must be escaped.
    // JSON stream: "hello\"world" -> C++: "\"hello\\\"world\""
    client.setMockData("  \"hello\\\"world\"  \"simple\" ");
    Utils::JsonStreamReader r(client);
    
    char buf[64];
    TEST_ASSERT_TRUE(r.readString(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("hello\"world", buf);
    
    TEST_ASSERT_TRUE(r.readString(buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("simple", buf);
}

void test_reader_numbers() {
    NetworkClient client;
    client.setMockData(" 123 -456 0");
    Utils::JsonStreamReader r(client);
    
    int64_t val;
    TEST_ASSERT_TRUE(r.readInt64(val));
    TEST_ASSERT_EQUAL_INT64(123, val);
    
    TEST_ASSERT_TRUE(r.readInt64(val));
    TEST_ASSERT_EQUAL_INT64(-456, val);
    
    TEST_ASSERT_TRUE(r.readInt64(val));
    TEST_ASSERT_EQUAL_INT64(0, val);
}

void test_reader_skip_complex() {
    NetworkClient client;
    // Skip nested: { "a": [1, {"x":2}], "b": "c" }
    client.setMockData(" { \"a\": [1, {\"x\":2}], \"b\": \"c\" } true ");
    Utils::JsonStreamReader r(client);
    
    TEST_ASSERT_TRUE(r.skipValue()); // Should skip entire object
    
    bool b;
    TEST_ASSERT_TRUE(r.readBool(b)); // Should read "true"
    TEST_ASSERT_TRUE(b);
}

// ============================================================================
// Writer Tests
// ============================================================================

void test_writer_primitives() {
    httpd_req_t req = nullptr;
    Utils::JsonResponseWriter w(&req);
    
    w.beginResponse();
    TEST_ASSERT_EQUAL_STRING("application/json", g_responseType.c_str());
    
    w.value(true);
    w.raw(",");
    w.value(false);
    w.raw(",");
    w.value(123);
    w.raw(",");
    w.value(12.34f, 2);
    w.finish();
    
    TEST_ASSERT_EQUAL_STRING("true,false,123,12.34", g_responseBuffer.c_str());
}

void test_writer_disables_caching_for_api_paths() {
    httpd_req_t req{};
    req.uri = "/api/test";
    Utils::JsonResponseWriter w(&req);

    TEST_ASSERT_TRUE(w.beginResponse());
    TEST_ASSERT_EQUAL_STRING("Cache-Control", g_responseHeaderField.c_str());
    TEST_ASSERT_EQUAL_STRING("no-store", g_responseHeaderValue.c_str());
}

void test_writer_escaping() {
    httpd_req_t req = nullptr;
    Utils::JsonResponseWriter w(&req);
    
    // Input: foo"bar\baz
    // JSON Output: "foo\"bar\\baz"
    w.string("foo\"bar\\baz");
    TEST_ASSERT_TRUE(w.finish());
    TEST_ASSERT_EQUAL_STRING("\"foo\\\"bar\\\\baz\"", g_responseBuffer.c_str());
    
    g_responseBuffer.clear();
    // Input: A<newline>B
    // JSON Output: "A\u000aB"
    w.string("A\nB");
    TEST_ASSERT_TRUE(w.finish());
    TEST_ASSERT_EQUAL_STRING("\"A\\u000aB\"", g_responseBuffer.c_str());
}

void test_writer_object() {
    httpd_req_t req = nullptr;
    Utils::JsonResponseWriter w(&req);
    
    w.raw("{");
    w.key("id"); w.value(1); w.raw(",");
    w.key("name"); w.string("Test");
    w.raw("}");
    TEST_ASSERT_TRUE(w.finish());
    
    TEST_ASSERT_EQUAL_STRING("{\"id\":1,\"name\":\"Test\"}", g_responseBuffer.c_str());
}

void test_parse_json_body_rejects_oversized_payload() {
    PsychicRequest request;
    request.setBody(std::string(65, 'x'));
    SYSTEM::SpiRamJsonDocument doc(64);

    const esp_err_t err = Response::parseJsonBody(&request, doc, 64);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(413, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Input::PAYLOAD_TOO_LARGE) != std::string::npos);
}

void test_parse_json_body_rejects_invalid_json() {
    PsychicRequest request;
    request.setBody("{");
    SYSTEM::SpiRamJsonDocument doc(256);

    const esp_err_t err = Response::parseJsonBody(&request, doc, 256);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(400, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Input::INVALID_JSON) != std::string::npos);
}

void test_parse_json_body_rejects_allocator_overflow_without_unbounded_growth() {
    std::string body = "[";
    for (size_t i = 0; i < 300; ++i) {
        if (i > 0) {
            body += ",";
        }
        body += "1";
    }
    body += "]";

    PsychicRequest request;
    request.setBody(body);
    constexpr size_t kDocLimit = 64;
    SYSTEM::SpiRamJsonDocument doc(kDocLimit);

    const esp_err_t err = Response::parseJsonBody(&request, doc, body.size() + 1);

    TEST_ASSERT_EQUAL(ESP_OK, err);
    TEST_ASSERT_EQUAL(413, request.lastStatusCode);
    TEST_ASSERT_TRUE(request.lastResponseBody.find(ErrorCodes::Input::PAYLOAD_TOO_LARGE) != std::string::npos);
    TEST_ASSERT_TRUE(doc.overflowed());
    TEST_ASSERT_LESS_OR_EQUAL(SYSTEM::SpiRamJsonDocument::effectiveLimitFor(kDocLimit),
                              doc.peakAllocatedBytes());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_reader_literals);
    RUN_TEST(test_reader_string);
    RUN_TEST(test_reader_numbers);
    RUN_TEST(test_reader_skip_complex);
    
    RUN_TEST(test_writer_primitives);
    RUN_TEST(test_writer_disables_caching_for_api_paths);
    RUN_TEST(test_writer_escaping);
    RUN_TEST(test_writer_object);
    RUN_TEST(test_parse_json_body_rejects_oversized_payload);
    RUN_TEST(test_parse_json_body_rejects_invalid_json);
    RUN_TEST(test_parse_json_body_rejects_allocator_overflow_without_unbounded_growth);
    return UNITY_END();
}
