#include <unity.h>

#include <cstring>
#include <string>

#include <esp_heap_caps.h>

#include "../../src/api/common/JwtAuthenticator.cpp"

namespace LOG {
Settings Logging::_settings;
void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}
void Logging::logSection(const char* title) { (void)title; }
void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}
esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}
} // namespace LOG

namespace {

class TestSecurityManager final : public SecurityManager {
public:
    Authentication nextAuth{};
    std::string lastToken;

    Authentication authenticateJWT(const char* jwt) override {
        lastToken = jwt ? jwt : "";
        return nextAuth;
    }
};

httpd_req_t makeReq(const char* query) {
    httpd_req_t req{};
    req.method = HTTP_GET;
    req.user_ctx = const_cast<char*>(query);
    return req;
}

httpd_req_t makeReqWithCookie(const char* cookie) {
    httpd_req_t req{};
    req.method = HTTP_GET;
    req.handle = const_cast<char*>(cookie);
    return req;
}

} // namespace

extern "C" size_t httpd_req_get_url_query_len(httpd_req_t* req) {
    if (!req || !req->user_ctx) {
        return 0;
    }
    return strlen(static_cast<const char*>(req->user_ctx));
}

extern "C" esp_err_t httpd_req_get_url_query_str(httpd_req_t* req, char* buf, size_t len) {
    if (!req || !req->user_ctx || !buf || len == 0) {
        return ESP_FAIL;
    }

    const char* query = static_cast<const char*>(req->user_ctx);
    const size_t queryLen = strlen(query);
    if (queryLen + 1 > len) {
        return ESP_FAIL;
    }

    memcpy(buf, query, queryLen + 1);
    return ESP_OK;
}

extern "C" esp_err_t httpd_query_key_value(const char* qry, const char* key, char* val, size_t val_size) {
    if (!qry || !key || !val || val_size == 0) {
        return ESP_FAIL;
    }

    const std::string query(qry);
    const std::string needle = std::string(key) + "=";
    size_t start = query.find(needle);
    if (start == std::string::npos) {
        return ESP_FAIL;
    }

    start += needle.size();
    const size_t end = query.find('&', start);
    const std::string value = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
    if (value.size() + 1 > val_size) {
        return ESP_FAIL;
    }

    memcpy(val, value.c_str(), value.size() + 1);
    return ESP_OK;
}

extern "C" esp_err_t httpd_req_get_cookie_val(httpd_req_t* req,
                                               const char* cookie_name,
                                               char* val,
                                               size_t* val_size) {
    if (!req || !req->handle || !cookie_name || !val || !val_size) {
        return ESP_ERR_NOT_FOUND;
    }

    const std::string cookieHeader(static_cast<const char*>(req->handle));
    const std::string needle = std::string(cookie_name) + "=";
    size_t start = cookieHeader.find(needle);
    if (start == std::string::npos) {
        return ESP_ERR_NOT_FOUND;
    }

    start += needle.size();
    const size_t end = cookieHeader.find(';', start);
    const std::string value = cookieHeader.substr(start, end == std::string::npos ? std::string::npos : end - start);

    if (value.size() + 1 > *val_size) {
        *val_size = value.size() + 1;
        return ESP_ERR_HTTPD_RESULT_TRUNC;
    }

    memcpy(val, value.c_str(), value.size() + 1);
    *val_size = value.size() + 1;
    return ESP_OK;
}

void setUp(void) {}
void tearDown(void) {}

void test_authenticate_rejects_missing_security_manager() {
    API::JwtAuthenticator auth(nullptr);

    httpd_req_t req = makeReqWithCookie("access_token=token");
    TEST_ASSERT_FALSE(auth.authenticate(&req));
}

void test_authenticate_rejects_missing_access_token_cookie() {
    TestSecurityManager securityManager;
    API::JwtAuthenticator auth(&securityManager);

    httpd_req_t req = makeReq(nullptr);
    TEST_ASSERT_FALSE(auth.authenticate(&req));
}

void test_authenticate_rejects_query_string_token_without_cookie() {
    TestSecurityManager securityManager;
    API::JwtAuthenticator auth(&securityManager);

    httpd_req_t req = makeReq("access_token=query-token");
    TEST_ASSERT_FALSE(auth.authenticate(&req));
}

void test_authenticate_rejects_invalid_token() {
    TestSecurityManager securityManager;
    securityManager.nextAuth = Authentication(false, false);
    API::JwtAuthenticator auth(&securityManager);

    httpd_req_t req = makeReqWithCookie("access_token=bad-token");
    TEST_ASSERT_FALSE(auth.authenticate(&req));
    TEST_ASSERT_EQUAL_STRING("bad-token", securityManager.lastToken.c_str());
}

void test_authenticate_rejects_predicate_failure() {
    TestSecurityManager securityManager;
    securityManager.nextAuth = Authentication(true, false, "viewer");
    API::JwtAuthenticator auth(&securityManager, AuthenticationPredicates::IS_ADMIN);

    httpd_req_t req = makeReqWithCookie("access_token=viewer-token");
    TEST_ASSERT_FALSE(auth.authenticate(&req));
}

void test_authenticate_accepts_valid_token() {
    TestSecurityManager securityManager;
    securityManager.nextAuth = Authentication(true, true, "admin");
    API::JwtAuthenticator auth(&securityManager, AuthenticationPredicates::IS_ADMIN);

    httpd_req_t req = makeReqWithCookie("foo=bar; access_token=good-token; theme=dark");
    TEST_ASSERT_TRUE(auth.authenticate(&req));
    TEST_ASSERT_EQUAL_STRING("good-token", securityManager.lastToken.c_str());
}

void test_authenticate_accepts_cookie_token_without_query_string() {
    TestSecurityManager securityManager;
    securityManager.nextAuth = Authentication(true, true, "admin");
    API::JwtAuthenticator auth(&securityManager, AuthenticationPredicates::IS_ADMIN);

    httpd_req_t req = makeReqWithCookie("foo=bar; access_token=cookie-token; theme=dark");
    TEST_ASSERT_TRUE(auth.authenticate(&req));
    TEST_ASSERT_EQUAL_STRING("cookie-token", securityManager.lastToken.c_str());
}

void test_authenticate_prefers_cookie_token_over_query_string() {
    TestSecurityManager securityManager;
    securityManager.nextAuth = Authentication(true, true, "admin");
    API::JwtAuthenticator auth(&securityManager, AuthenticationPredicates::IS_ADMIN);

    httpd_req_t req = makeReq("access_token=query-token");
    req.handle = const_cast<char*>("access_token=cookie-token");

    TEST_ASSERT_TRUE(auth.authenticate(&req));
    TEST_ASSERT_EQUAL_STRING("cookie-token", securityManager.lastToken.c_str());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_authenticate_rejects_missing_security_manager);
    RUN_TEST(test_authenticate_rejects_missing_access_token_cookie);
    RUN_TEST(test_authenticate_rejects_query_string_token_without_cookie);
    RUN_TEST(test_authenticate_rejects_invalid_token);
    RUN_TEST(test_authenticate_rejects_predicate_failure);
    RUN_TEST(test_authenticate_accepts_valid_token);
    RUN_TEST(test_authenticate_accepts_cookie_token_without_query_string);
    RUN_TEST(test_authenticate_prefers_cookie_token_over_query_string);
    return UNITY_END();
}
