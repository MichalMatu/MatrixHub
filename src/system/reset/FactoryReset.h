#pragma once

class ServiceRegistry;

namespace SYSTEM {

// Performs a factory reset: clears app preferences and formats LittleFS, then restarts.
void performFactoryReset(ServiceRegistry& registry);

}
