#include <security/SecuritySettings.h>

#include <security/PasswordHasher.h>
#include "../../src/system/logging/Logging.h"

#include <esp_system.h>
#include <time.h>
#include <utility>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#undef LOG_TAG
#define LOG_TAG "Security"

namespace {

constexpr std::string_view kHttpOriginId = "http";
constexpr size_t kJwtSecretBytes = 32;

bool isHttpOrigin(std::string_view originId)
{
    return originId == kHttpOriginId;
}

String bytesToHexString(const uint8_t* data, size_t len)
{
    String hex;
    hex.reserve(len * 2);
    static constexpr char kHexDigits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++)
    {
        const uint8_t value = data[i];
        hex += kHexDigits[value >> 4];
        hex += kHexDigits[value & 0x0F];
    }
    return hex;
}

String generatePersistentJwtSecret()
{
    uint8_t randomBytes[kJwtSecretBytes];
    esp_fill_random(randomBytes, sizeof(randomBytes));
    return bytesToHexString(randomBytes, sizeof(randomBytes));
}

String resolveJwtSecret(JsonObject &root, const SecuritySettings &settings)
{
    const String requestedSecret = root["jwt_secret"] | "";
    if (!requestedSecret.isEmpty() &&
        requestedSecret != FACTORY_JWT_SECRET &&
        requestedSecret != "random")
    {
        return requestedSecret;
    }

    if (!settings.jwtSecret.isEmpty())
    {
        return settings.jwtSecret;
    }

    // Keep the persisted JWT signing secret stable once created, but generate
    // it from esp_fill_random() on first boot/factory reset instead of the
    // framework's generic "#{random}" placeholder expansion.
    return generatePersistentJwtSecret();
}

const User* findUserByUsername(const std::list<User> &users, const String &username)
{
    for (const auto &user : users)
    {
        if (user.username == username)
        {
            return &user;
        }
    }

    return nullptr;
}

String resolveStoredCredential(const String &incomingPassword,
                               const User *previousUser,
                               std::string_view originId,
                               bool *passwordChanged,
                               bool *legacyCredentialMigrated)
{
    if (passwordChanged)
    {
        *passwordChanged = false;
    }
    if (legacyCredentialMigrated)
    {
        *legacyCredentialMigrated = false;
    }

    if (isHttpOrigin(originId))
    {
        if (previousUser && incomingPassword.isEmpty())
        {
            return previousUser->password;
        }

        if (passwordChanged)
        {
            *passwordChanged = !previousUser ||
                               !PasswordHasher::matchesStoredCredential(incomingPassword, previousUser->password);
        }
        return PasswordHasher::hashPassword(incomingPassword);
    }

    if (PasswordHasher::isHashedCredential(incomingPassword))
    {
        if (passwordChanged)
        {
            *passwordChanged = !previousUser || previousUser->password != incomingPassword;
        }
        return incomingPassword;
    }

    if (legacyCredentialMigrated)
    {
        *legacyCredentialMigrated = true;
    }
    if (passwordChanged)
    {
        *passwordChanged = !previousUser ||
                           !PasswordHasher::matchesStoredCredential(incomingPassword, previousUser->password);
    }
    return PasswordHasher::hashPassword(incomingPassword);
}

} // namespace

void SecuritySettings::readForApi(SecuritySettings &settings, JsonObject &root)
{
    // The signing secret is write-only through the HTTP API. Exposing it in
    // the admin UI response turns every browser/session dump into a bearer
    // token minting capability.
    root["jwt_secret"] = "";
    root["jwt_secret_configured"] = !settings.jwtSecret.isEmpty();

    JsonArray usersArray = root["users"].to<JsonArray>();
    for (const User &user : settings.users)
    {
        JsonObject userRoot = usersArray.add<JsonObject>();
        userRoot["username"] = user.username;
        userRoot["password"] = "";
        userRoot["admin"] = user.admin;
    }
}

void SecuritySettings::readForStorage(SecuritySettings &settings, JsonObject &root)
{
    root["jwt_secret"] = settings.jwtSecret;

    JsonArray usersArray = root["users"].to<JsonArray>();
    for (const User &user : settings.users)
    {
        JsonObject userRoot = usersArray.add<JsonObject>();
        userRoot["username"] = user.username;
        userRoot["password"] = user.password;
        userRoot["admin"] = user.admin;
    }
}

StateUpdateResult SecuritySettings::update(JsonObject &root, SecuritySettings &settings, std::string_view originId)
{
    const String nextJwtSecret = resolveJwtSecret(root, settings);

    std::list<User> previousUsers = std::move(settings.users);
    std::list<User> nextUsers;
    bool pendingCredentialMigration = false;

    if (root["users"].is<JsonArray>())
    {
        for (JsonVariant userVariant : root["users"].as<JsonArray>())
        {
            const String username = userVariant["username"] | "";
            const String incomingPassword = userVariant["password"] | "";
            const bool isAdmin = userVariant["admin"] | false;

            const User *previousUser = findUserByUsername(previousUsers, username);

            bool passwordChanged = false;
            bool legacyCredentialMigrated = false;
            const String storedCredential =
                resolveStoredCredential(incomingPassword,
                                        previousUser,
                                        originId,
                                        &passwordChanged,
                                        &legacyCredentialMigrated);

            // Yield to idle task to feed TWDT during potentially heavy PBKDF2 hashing
            vTaskDelay(pdMS_TO_TICKS(10));

            if (storedCredential.isEmpty())
            {
                LOGE("Failed to derive password hash for user '%s'", username.c_str());
                settings.users = std::move(previousUsers);
                settings.pendingCredentialMigration = false;
                return StateUpdateResult::ERROR;
            }

            User newUser(username, storedCredential, isAdmin);
            if (previousUser)
            {
                if (passwordChanged || previousUser->admin != isAdmin)
                {
                    newUser.validAfter = time(nullptr);
                }
                else
                {
                    newUser.validAfter = previousUser->validAfter;
                }
            }

            pendingCredentialMigration = pendingCredentialMigration || legacyCredentialMigrated;
            nextUsers.push_back(std::move(newUser));
        }
    }

    bool hasAdmin = false;
    for (const auto &user : nextUsers)
    {
        if (user.admin)
        {
            hasAdmin = true;
            break;
        }
    }

    if (!hasAdmin)
    {
        // DEV-ONLY SECURITY EXCEPTION:
        // If configuration removes every admin, restore the known recovery
        // admin instead of failing closed so development boards are not
        // accidentally bricked during bring-up. We accept the predictable
        // credential risk for the active dev phase. Production must replace
        // this with an explicit recovery flow.
        LOGW("No admin found! Restoring default admin for recovery.");
        const String recoveryCredential = PasswordHasher::hashPassword(FACTORY_ADMIN_PASSWORD);
        if (recoveryCredential.isEmpty())
        {
            LOGE("Failed to derive recovery admin credential hash");
            settings.users = std::move(previousUsers);
            settings.pendingCredentialMigration = false;
            return StateUpdateResult::ERROR;
        }

        nextUsers.push_back(User(FACTORY_ADMIN_USERNAME, recoveryCredential, true));
    }

    settings.jwtSecret = nextJwtSecret;
    settings.users = std::move(nextUsers);
    settings.pendingCredentialMigration = pendingCredentialMigration;
    return StateUpdateResult::CHANGED;
}
