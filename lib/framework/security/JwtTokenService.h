#ifndef JwtTokenService_h
#define JwtTokenService_h

#include <security/ArduinoJsonJWT.h>
#include <security/SecurityManager.h>

class JwtTokenService
{
public:
    explicit JwtTokenService(const String &factorySecret);

    void configureSecret(const String &configuredSecret);

    Authentication authenticateJWT(const char *jwt, const std::list<User> &users);
    Authentication authenticateCredentials(const String &username,
                                           const String &password,
                                           const std::list<User> &users) const;

    String generateJWT(User *user);

private:
    boolean validatePayload(JsonObject &parsedPayload, const User *user) const;

    ArduinoJsonJWT _jwtHandler;
};

#endif // end JwtTokenService_h
