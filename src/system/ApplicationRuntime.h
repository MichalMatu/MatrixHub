#pragma once

#include <cstdint>

#include <PsychicHttpServer.h>
#include <core/ESP32SvelteKit.h>

class ButtonHandler;
class PsychicHttpsServer;
class ServiceRegistry;

namespace SYSTEM {
enum class ShutdownReason : uint8_t;
namespace NETWORK {
class HttpRedirectServer;
}
}

namespace APP_INTERNAL {

void restartRuntimeFault(const char* logReason,
                         SYSTEM::ShutdownReason shutdownReason);

void runBootSequence(PsychicHttpsServer& server,
                     ESP32SvelteKit& framework,
                     SYSTEM::NETWORK::HttpRedirectServer& httpRedirectServer,
                     ServiceRegistry& services,
                     ButtonHandler& buttonHandler,
                     SemaphoreHandle_t networkMutex,
                     SemaphoreHandle_t notifMutex);

void runLoopCore(ServiceRegistry& services,
                 ButtonHandler& buttonHandler,
                 ESP32SvelteKit& framework);

}  // namespace APP_INTERNAL
