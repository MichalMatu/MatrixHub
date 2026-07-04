#pragma once

class ServiceRegistry;
class ButtonHandler;

namespace SYSTEM {

class MonitoringInitializer {
public:
    static void initialize(ServiceRegistry& services, ButtonHandler& buttonHandler);
};

}  // namespace SYSTEM
