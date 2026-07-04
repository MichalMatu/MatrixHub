#include <unity.h>
#include <ArduinoJson.h>

#ifndef tskIDLE_PRIORITY
#define tskIDLE_PRIORITY 0
#endif

#include "../../lib/framework/security/PasswordHasher.cpp"
#include "../../lib/framework/security/SecuritySettings.cpp"
#include "../../lib/framework/utils/SettingValue.cpp"

namespace LOG {
Settings Logging::_settings{};

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::logSection(const char* title) {
    (void)title;
}

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

void setUp(void) {}
void tearDown(void) {}

void test_update_from_storage_hashes_legacy_password_and_marks_pending_migration() {
    SecuritySettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["jwt_secret"] = "secret";
    JsonObject user = root["users"].to<JsonArray>().add<JsonObject>();
    user["username"] = "alice";
    user["password"] = "plain-secret";
    user["admin"] = true;

    const StateUpdateResult result = SecuritySettings::update(root, settings, "/config/securitySettings.json");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL(1, settings.users.size());
    TEST_ASSERT_TRUE(settings.pendingCredentialMigration);
    TEST_ASSERT_TRUE(PasswordHasher::isHashedCredential(settings.users.front().password));
    TEST_ASSERT_TRUE(PasswordHasher::matchesStoredCredential("plain-secret", settings.users.front().password));
}

void test_read_for_api_hides_passwords_and_jwt_secret_from_http_payload() {
    SecuritySettings settings;
    settings.jwtSecret = "secret";
    settings.users.push_back(User("alice", PasswordHasher::hashPassword("top-secret"), true));

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    SecuritySettings::readForApi(settings, root);

    TEST_ASSERT_EQUAL_STRING("", root["jwt_secret"] | "");
    TEST_ASSERT_TRUE(root["jwt_secret_configured"].as<bool>());
    TEST_ASSERT_TRUE(root["users"].is<JsonArray>());
    TEST_ASSERT_EQUAL(1, root["users"].as<JsonArray>().size());
    TEST_ASSERT_EQUAL_STRING("alice", root["users"][0]["username"] | "");
    TEST_ASSERT_EQUAL_STRING("", root["users"][0]["password"] | "");
    TEST_ASSERT_TRUE(root["users"][0]["admin"].as<bool>());
}

void test_read_for_storage_preserves_hashed_credential() {
    SecuritySettings settings;
    settings.jwtSecret = "secret";
    const String hashedPassword = PasswordHasher::hashPassword("top-secret");
    settings.users.push_back(User("alice", hashedPassword, true));

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    SecuritySettings::readForStorage(settings, root);

    TEST_ASSERT_EQUAL_STRING(hashedPassword.c_str(), root["users"][0]["password"] | "");
}

void test_update_from_http_keeps_existing_hash_when_password_is_blank() {
    SecuritySettings settings;
    const String hashedPassword = PasswordHasher::hashPassword("keep-me");
    settings.users.push_back(User("alice", hashedPassword, true));
    settings.users.front().validAfter = 12345;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    root["jwt_secret"] = "secret";
    JsonObject user = root["users"].to<JsonArray>().add<JsonObject>();
    user["username"] = "alice";
    user["password"] = "";
    user["admin"] = true;

    const StateUpdateResult result = SecuritySettings::update(root, settings, "http");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_FALSE(settings.pendingCredentialMigration);
    TEST_ASSERT_EQUAL(1, settings.users.size());
    TEST_ASSERT_EQUAL_STRING(hashedPassword.c_str(), settings.users.front().password.c_str());
    TEST_ASSERT_EQUAL(static_cast<unsigned long>(12345), static_cast<unsigned long>(settings.users.front().validAfter));
}

void test_update_generates_persistent_jwt_secret_when_missing() {
    SecuritySettings settings;

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonObject user = root["users"].to<JsonArray>().add<JsonObject>();
    user["username"] = "alice";
    user["password"] = "plain-secret";
    user["admin"] = true;

    const StateUpdateResult result = SecuritySettings::update(root, settings, "/config/securitySettings.json");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_TRUE(settings.jwtSecret.length() > 0);
    TEST_ASSERT_TRUE(strcmp(FACTORY_JWT_SECRET, settings.jwtSecret.c_str()) != 0);
    TEST_ASSERT_EQUAL(static_cast<unsigned long>(64), static_cast<unsigned long>(settings.jwtSecret.length()));
}

void test_update_without_jwt_secret_keeps_existing_secret() {
    SecuritySettings settings;
    settings.jwtSecret = "keep-existing-secret";

    JsonDocument doc;
    JsonObject root = doc.to<JsonObject>();
    JsonObject user = root["users"].to<JsonArray>().add<JsonObject>();
    user["username"] = "alice";
    user["password"] = "plain-secret";
    user["admin"] = true;

    const StateUpdateResult result = SecuritySettings::update(root, settings, "http");

    TEST_ASSERT_EQUAL(static_cast<int>(StateUpdateResult::CHANGED), static_cast<int>(result));
    TEST_ASSERT_EQUAL_STRING("keep-existing-secret", settings.jwtSecret.c_str());
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_update_from_storage_hashes_legacy_password_and_marks_pending_migration);
    RUN_TEST(test_read_for_api_hides_passwords_and_jwt_secret_from_http_payload);
    RUN_TEST(test_read_for_storage_preserves_hashed_credential);
    RUN_TEST(test_update_from_http_keeps_existing_hash_when_password_is_blank);
    RUN_TEST(test_update_generates_persistent_jwt_secret_when_missing);
    RUN_TEST(test_update_without_jwt_secret_keeps_existing_secret);
    return UNITY_END();
}
