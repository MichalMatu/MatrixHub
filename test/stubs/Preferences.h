#pragma once

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace TEST_STUBS::PREFERENCES {
inline std::vector<std::string> operations;
inline std::vector<std::string> clearedNamespaces;
inline std::set<std::string> beginFailures;
inline std::string lastNamespace;
inline bool lastReadOnly = false;

inline void reset() {
    operations.clear();
    clearedNamespaces.clear();
    beginFailures.clear();
    lastNamespace.clear();
    lastReadOnly = false;
}
}

class Preferences {
public:
    bool begin(const char* name, bool readOnly = false, const char* partition_label = nullptr) {
        (void)partition_label;
        TEST_STUBS::PREFERENCES::lastNamespace = name ? name : "";
        TEST_STUBS::PREFERENCES::lastReadOnly = readOnly;
        TEST_STUBS::PREFERENCES::operations.push_back(
            std::string("begin:") + TEST_STUBS::PREFERENCES::lastNamespace);
        return TEST_STUBS::PREFERENCES::beginFailures.count(TEST_STUBS::PREFERENCES::lastNamespace) == 0;
    }
    void end() {
        TEST_STUBS::PREFERENCES::operations.push_back(
            std::string("end:") + TEST_STUBS::PREFERENCES::lastNamespace);
    }

    bool clear() {
        TEST_STUBS::PREFERENCES::operations.push_back(
            std::string("clear:") + TEST_STUBS::PREFERENCES::lastNamespace);
        TEST_STUBS::PREFERENCES::clearedNamespaces.push_back(TEST_STUBS::PREFERENCES::lastNamespace);
        return true;
    }

    size_t putBool(const char* key, bool value) { return 1; }
    size_t putUInt(const char* key, uint32_t value) { return 4; }
    size_t putUChar(const char* key, uint8_t value) { return 1; }
    size_t putFloat(const char* key, float value) { return 4; }
    
    bool getBool(const char* key, bool defaultValue = false) { return defaultValue; }
    float getFloat(const char* key, float defaultValue = 0.0f) { return defaultValue; }
};
