#pragma once

namespace SYSTEM {
class SpiRamJsonDocument;
}

namespace CONFIG::Serialization {

bool loadConfigSections(SYSTEM::SpiRamJsonDocument& doc);
bool loadPsramOnlyConfigSections(SYSTEM::SpiRamJsonDocument& doc);

}  // namespace CONFIG::Serialization
