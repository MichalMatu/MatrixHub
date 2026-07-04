#pragma once

#include <Arduino.h>
#include <vector>
#include <FS.h>
#include <LittleFS.h>
#include "../model/MacroDefinitions.h"

namespace MACROS {

class MacroRepository {
public:
    static void setFsMutex(SemaphoreHandle_t mutex);
    static PsramVector<PsramString> listScripts();
    static bool deleteScript(const char* filename);
    static bool saveScript(const char* filename, const char* content);
    static PsramString getScriptContent(const char* filename);
    static bool scriptExists(const char* filename);

private:
    static SemaphoreHandle_t _fsMutex;
};

}
