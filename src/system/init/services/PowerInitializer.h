#pragma once

class ServiceRegistry;

namespace SYSTEM {

class PowerInitializer {
public:
    static void initialize(ServiceRegistry& services);
};

}  // namespace SYSTEM
