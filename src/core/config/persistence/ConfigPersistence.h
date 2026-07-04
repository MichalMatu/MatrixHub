#pragma once

#include <FS.h>

namespace SYSTEM {
class SpiRamJsonDocument;
}

namespace CONFIG::Persistence {

bool readConfigDocument(FS& fs, SYSTEM::SpiRamJsonDocument& doc, const char* phase);
bool writeConfigDocumentAtomically(FS& fs, SYSTEM::SpiRamJsonDocument& doc);
void deleteConfigFile(FS& fs);

}  // namespace CONFIG::Persistence
