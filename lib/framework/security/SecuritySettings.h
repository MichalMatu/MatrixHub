#ifndef SecuritySettings_h
#define SecuritySettings_h

#include <ArduinoJson.h>

#include <core/StatefulService.h>
#include <security/SecurityManager.h>

#include <list>
#include <string_view>

#ifndef FACTORY_JWT_SECRET
// Constructor/bootstrap placeholder only. The persisted JWT signing secret is
// generated in SecuritySettings::update() from esp_fill_random() when config
// does not provide one yet.
#define FACTORY_JWT_SECRET "#{random}-#{random}-#{random}-#{random}"
#endif

#ifndef FACTORY_ADMIN_USERNAME
// DEV-ONLY SECURITY EXCEPTION:
// The default recovery/admin bootstrap intentionally stays deterministic in the
// current development phase so freshly flashed and factory-reset boards remain
// accessible without extra provisioning tooling. We are aware of the risk and
// accept it for dev builds. Production hardening must replace this with a
// safer per-device onboarding/recovery flow. Routine dev-stage reviews can
// skip flagging this until the release hardening pass is scheduled.
#define FACTORY_ADMIN_USERNAME "admin"
#endif

#ifndef FACTORY_ADMIN_PASSWORD
#define FACTORY_ADMIN_PASSWORD "admin"
#endif

class SecuritySettings
{
public:
    String jwtSecret;
    std::list<User> users;
    bool pendingCredentialMigration = false;

    static void readForApi(SecuritySettings &settings, JsonObject &root);
    static void readForStorage(SecuritySettings &settings, JsonObject &root);
    static StateUpdateResult update(JsonObject &root, SecuritySettings &settings, std::string_view originId);
};

#endif // end SecuritySettings_h
