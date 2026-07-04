#include <unity.h>

#include "../../lib/framework/security/PasswordHasher.cpp"

void setUp(void) {}
void tearDown(void) {}

void test_hash_password_produces_pbkdf2_hash_and_verifies() {
    const String hash = PasswordHasher::hashPassword("secret123");

    TEST_ASSERT_FALSE(hash.isEmpty());
    TEST_ASSERT_TRUE(PasswordHasher::isHashedCredential(hash));
    TEST_ASSERT_TRUE(PasswordHasher::verifyPassword("secret123", hash));
    TEST_ASSERT_FALSE(PasswordHasher::verifyPassword("wrong", hash));
}

void test_matches_stored_credential_supports_legacy_plaintext() {
    TEST_ASSERT_TRUE(PasswordHasher::matchesStoredCredential("admin", "admin"));
    TEST_ASSERT_FALSE(PasswordHasher::matchesStoredCredential("wrong", "admin"));
}

void test_hash_password_uses_unique_salt_per_invocation() {
    const String first = PasswordHasher::hashPassword("secret123");
    const String second = PasswordHasher::hashPassword("secret123");

    TEST_ASSERT_FALSE(first == second);
    TEST_ASSERT_TRUE(PasswordHasher::verifyPassword("secret123", first));
    TEST_ASSERT_TRUE(PasswordHasher::verifyPassword("secret123", second));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_hash_password_produces_pbkdf2_hash_and_verifies);
    RUN_TEST(test_matches_stored_credential_supports_legacy_plaintext);
    RUN_TEST(test_hash_password_uses_unique_salt_per_invocation);
    return UNITY_END();
}
