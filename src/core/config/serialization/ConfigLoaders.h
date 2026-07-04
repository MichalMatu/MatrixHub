#pragma once

namespace SYSTEM {
class SpiRamJsonDocument;
}

namespace CONFIG::Serialization {

void loadConfigSections(SYSTEM::SpiRamJsonDocument& doc);
void loadPsramOnlyConfigSections(SYSTEM::SpiRamJsonDocument& doc);

}  // namespace CONFIG::Serialization
