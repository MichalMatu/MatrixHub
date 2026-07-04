#pragma once

#include "../boot/BootTracker.h"

class ServiceRegistry;

namespace SYSTEM {

class ShutdownSequence {
public:
    /**
     * @brief Execute full clean shutdown sequence.
     * 
     * Stops services, saves state, and prepares hardware for deep sleep.
     * This is the counterpart to InitSequence.
     * 
     * @param registry The service registry providing access to all services.
     */
    static void execute(ServiceRegistry& registry);
    static void execute(ServiceRegistry& registry, ShutdownReason explicitReason);

private:
    static void stopBackgroundTasks(ServiceRegistry& registry);
    static void stopNetworkServices(ServiceRegistry& registry);
    static void persistState(ServiceRegistry& registry, ShutdownReason explicitReason);
    static void stopHardware(ServiceRegistry& registry);
};

} // namespace SYSTEM
