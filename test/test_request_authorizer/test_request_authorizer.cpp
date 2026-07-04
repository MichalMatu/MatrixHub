#include <unity.h>

#include "../../test/stubs/PsychicHttp.h"
#include "../../lib/framework/security/SecurityManager.h"
#include "../../src/system/logging/Logging.h"
#include "../../lib/framework/security/RequestAuthorizer.cpp"

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

RateLimiter::RateLimiter(size_t maxRequests, unsigned long windowSizeMs)
    : _maxRequests(maxRequests),
      _windowSizeMs(windowSizeMs),
      _lastCleanup(0),
      _mutex(nullptr) {
}

RateLimiter::~RateLimiter() = default;

bool RateLimiter::shouldAllow(uint32_t ip) {
    (void)ip;
    return true;
}

void RateLimiter::cleanup(bool force) {
    (void)force;
}

namespace {

String g_lastToken;

Authentication authenticateToken(const char* jwt) {
    g_lastToken = jwt ? jwt : "";
    return Authentication(User("admin", "", true));
}

void resetState() {
    g_lastToken = "";
}

} // namespace

void setUp(void) {
    resetState();
}

void tearDown(void) {}

void test_authenticate_request_accepts_bearer_header_only() {
    RequestAuthorizer authorizer(nullptr, authenticateToken);
    PsychicRequest request;
    request.setHeader("Authorization", "Bearer header-token");

    const Authentication auth = authorizer.authenticateRequest(&request);

    TEST_ASSERT_TRUE(auth.authenticated);
    TEST_ASSERT_EQUAL_STRING("header-token", g_lastToken.c_str());
}

void test_authenticate_request_rejects_query_token_without_header() {
    RequestAuthorizer authorizer(nullptr, authenticateToken);
    PsychicRequest request;
    request.setParam("access_token", "query-token");

    const Authentication auth = authorizer.authenticateRequest(&request);

    TEST_ASSERT_FALSE(auth.authenticated);
    TEST_ASSERT_TRUE(g_lastToken.isEmpty());
}

void test_authenticate_request_rejects_non_bearer_header() {
    RequestAuthorizer authorizer(nullptr, authenticateToken);
    PsychicRequest request;
    request.setHeader("Authorization", "Basic abc123");
    request.setParam("access_token", "query-token");

    const Authentication auth = authorizer.authenticateRequest(&request);

    TEST_ASSERT_FALSE(auth.authenticated);
    TEST_ASSERT_TRUE(g_lastToken.isEmpty());
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_authenticate_request_accepts_bearer_header_only);
    RUN_TEST(test_authenticate_request_rejects_query_token_without_header);
    RUN_TEST(test_authenticate_request_rejects_non_bearer_header);
    return UNITY_END();
}
