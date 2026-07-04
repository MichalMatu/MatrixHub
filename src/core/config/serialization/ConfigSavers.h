#pragma once

namespace ALARMS {
struct AlarmRulesSnapshot;
}

namespace SYSTEM {
class SpiRamJsonDocument;
}

namespace CONFIG::Serialization {

void buildConfigDocument(SYSTEM::SpiRamJsonDocument& doc,
                         const ALARMS::AlarmRulesSnapshot* alarmRulesOverride);

}  // namespace CONFIG::Serialization
