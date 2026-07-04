#include <unity.h>
#include <ArduinoJson.h>

#include <ctime>
#include <list>

namespace TEST_TIME {
inline time_t now = 0;
inline tm localTime{};
}

extern "C" time_t jwt_test_time(time_t *timer) {
    if (timer) {
        *timer = TEST_TIME::now;
    }
    return TEST_TIME::now;
}

extern "C" tm *jwt_test_localtime_r(const time_t *timer, tm *result) {
    (void)timer;
    if (!result) {
        return nullptr;
    }
    *result = TEST_TIME::localTime;
    return result;
}

#define time jwt_test_time
#define localtime_r jwt_test_localtime_r
#include "../../lib/framework/security/ArduinoJsonJWT.cpp"
#include "../../lib/framework/security/JwtTokenService.cpp"
#undef localtime_r
#undef time

namespace LOG {
void Logging::log(esp_log_level_t level, const char *tag, const char *fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::logSection(const char *title) {
    (void)title;
}

void Logging::logStackHwm(const char *taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}
}  // namespace LOG

bool PasswordHasher::matchesStoredCredential(const String &password, const String &storedCredential) {
    return storedCredential == password;
}

namespace {

void setMockYear(int year) {
    TEST_TIME::localTime = {};
    TEST_TIME::localTime.tm_year = year - 1900;
    TEST_TIME::localTime.tm_mon = 0;
    TEST_TIME::localTime.tm_mday = 1;
}

std::list<User> makeUsers() {
    return {User("admin", "secret", true)};
}

String buildLegacyTokenWithoutSessionId(const String &secret,
                                        const char *username,
                                        bool admin,
                                        time_t iat,
                                        time_t exp) {
    ArduinoJsonJWT handler(secret);
    JsonDocument doc;
    JsonObject payload = doc.to<JsonObject>();
    payload["username"] = username;
    payload["admin"] = admin;
    payload["iat"] = iat;
    payload["exp"] = exp;
    return handler.buildJWT(payload);
}

}  // namespace

void setUp(void) {}
void tearDown(void) {}

void test_current_boot_session_token_is_accepted_when_time_is_invalid() {
    TEST_TIME::now = 1234;
    setMockYear(1970);

    JwtTokenService service("secret");
    service.configureSecret("secret");
    auto users = makeUsers();
    auto user = users.begin();

    const String token = service.generateJWT(&(*user));
    const Authentication auth = service.authenticateJWT(token.c_str(), users);

    TEST_ASSERT_TRUE(auth.authenticated);
    TEST_ASSERT_NOT_NULL(auth.user.get());
    TEST_ASSERT_EQUAL_STRING("admin", auth.user->username.c_str());
}

void test_legacy_token_without_session_id_is_accepted_when_time_is_invalid() {
    TEST_TIME::now = 1234;
    setMockYear(1970);

    JwtTokenService service("secret");
    service.configureSecret("secret");
    auto users = makeUsers();

    const String token = buildLegacyTokenWithoutSessionId("secret", "admin", true, 1000, 2000);
    const Authentication auth = service.authenticateJWT(token.c_str(), users);

    TEST_ASSERT_TRUE(auth.authenticated);
    TEST_ASSERT_NOT_NULL(auth.user.get());
    TEST_ASSERT_EQUAL_STRING("admin", auth.user->username.c_str());
}

void test_legacy_token_exp_is_ignored_when_time_is_valid() {
    TEST_TIME::now = 5000;
    setMockYear(2026);

    JwtTokenService service("secret");
    service.configureSecret("secret");
    auto users = makeUsers();

    const String token = buildLegacyTokenWithoutSessionId("secret", "admin", true, 1000, 2000);
    const Authentication auth = service.authenticateJWT(token.c_str(), users);

    TEST_ASSERT_TRUE(auth.authenticated);
    TEST_ASSERT_NOT_NULL(auth.user.get());
    TEST_ASSERT_EQUAL_STRING("admin", auth.user->username.c_str());
}

void test_token_is_rejected_when_issued_before_valid_after() {
    TEST_TIME::now = 5000;
    setMockYear(2026);

    JwtTokenService service("secret");
    service.configureSecret("secret");
    auto users = makeUsers();
    auto user = users.begin();

    const String token = service.generateJWT(&(*user));
    user->validAfter = 6000;
    const Authentication auth = service.authenticateJWT(token.c_str(), users);

    TEST_ASSERT_FALSE(auth.authenticated);
}

void test_token_is_rejected_after_revocation_stamp_changes_in_same_second() {
    TEST_TIME::now = 5000;
    setMockYear(2026);

    JwtTokenService service("secret");
    service.configureSecret("secret");
    auto users = makeUsers();
    auto user = users.begin();

    const String oldToken = service.generateJWT(&(*user));
    user->validAfter = 5000;

    const Authentication oldAuth = service.authenticateJWT(oldToken.c_str(), users);
    TEST_ASSERT_FALSE(oldAuth.authenticated);

    const String newToken = service.generateJWT(&(*user));
    const Authentication newAuth = service.authenticateJWT(newToken.c_str(), users);
    TEST_ASSERT_TRUE(newAuth.authenticated);
    TEST_ASSERT_NOT_NULL(newAuth.user.get());
    TEST_ASSERT_EQUAL_STRING("admin", newAuth.user->username.c_str());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_current_boot_session_token_is_accepted_when_time_is_invalid);
    RUN_TEST(test_legacy_token_without_session_id_is_accepted_when_time_is_invalid);
    RUN_TEST(test_legacy_token_exp_is_ignored_when_time_is_valid);
    RUN_TEST(test_token_is_rejected_when_issued_before_valid_after);
    RUN_TEST(test_token_is_rejected_after_revocation_stamp_changes_in_same_second);
    return UNITY_END();
}
