#ifndef PasswordHasher_h
#define PasswordHasher_h

#include <Arduino.h>

class PasswordHasher
{
public:
    static String hashPassword(const String &password);
    static bool verifyPassword(const String &password, const String &storedHash);
    static bool matchesStoredCredential(const String &password, const String &storedCredential);
    static bool isHashedCredential(const String &storedCredential);
};

#endif // end PasswordHasher_h
