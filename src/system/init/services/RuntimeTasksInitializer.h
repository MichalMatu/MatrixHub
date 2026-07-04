#pragma once

class ServiceRegistry;

namespace SYSTEM {

class RuntimeTasksInitializer {
public:
    static void initialize(ServiceRegistry& services);
};

}  // namespace SYSTEM
