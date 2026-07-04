#pragma once
#include <NetworkClient.h>
#include <Arduino.h>

namespace Utils {

class JsonStreamReader {
public:
    explicit JsonStreamReader(NetworkClient& s);

    // Basic I/O
    int peek();
    int read();
    int readSkipWs();
    bool readExact(char expected);
    
    // Helper to skip whitespace and peek next char
    bool skipWsPeek(int& out);

    // Primitive readers
    bool readBool(bool& out);
    bool readInt64(int64_t& out);
    bool readString(char* out, size_t outSize); // Reads "string" and unescapes standard chars

    // Skippers
    bool skipValue(int depth = 0); // Skips any JSON value (object, array, string, number, bool, null)
    
    // Specialized skippers (usually called by skipValue)
    bool skipString();
    bool skipObject(int depth = 0);
    bool skipArray(int depth = 0);
    bool skipNumber();

private:
    NetworkClient& _stream;

    bool readLiteral(const char* lit);
    static bool isHex(int c);
};

} // namespace Utils
