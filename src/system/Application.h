#pragma once

#include <PsychicHttpServer.h>
#include <core/ESP32SvelteKit.h>

#include "button/ButtonHandler.h"
#include "network/HttpRedirectServer.h"
#include "services/ServiceRegistry.h"

class PsychicHttpsServer;

// Forward declaration for friend
namespace SYSTEM {
namespace MEMORY {
    template <typename T, typename... Args>
    T* allocInPsram(Args&&... args);
}
}

class Application {
public:
  // Friend declaration for SystemAllocator to allow access to private constructor
  template <typename T, typename... Args>
  friend T* SYSTEM::MEMORY::allocInPsram(Args&&... args);

  static Application &instance();

  void setup(PsychicHttpsServer &server, ESP32SvelteKit &framework);
  void loop();

  // Entry-point escape hatch for framework callbacks that cannot receive user
  // context yet (for example factory reset hook). Keep normal runtime code on
  // explicit DI and avoid expanding use of this accessor.
  ServiceRegistry* getServiceRegistry() {
      return &_services;
  }

private:
  enum class LifecycleState {
    NotInitialized,
    Booting,
    Ready,
  };

  Application();
  static const char* stateName(LifecycleState state);
  void createMutexes();
  
  SemaphoreHandle_t _networkMutex{nullptr};
  SemaphoreHandle_t _notifMutex{nullptr};
  LifecycleState _state{LifecycleState::NotInitialized};

  // Services
  ServiceRegistry _services;
  // Runtime-owned input component. Button wiring lives in Phase 7, but the
  // lifetime is explicit here so we do not fall back to a hidden singleton.
  // If button behavior differs between boot and runtime, inspect Phase 7
  // bindings in MonitoringInitializer before suspecting ownership bugs here.
  ButtonHandler _buttonHandler;
  // Separate plain-HTTP shim. It only redirects users to HTTPS and does not own
  // the application routes, which keeps the real UI/API surface on TLS.
  SYSTEM::NETWORK::HttpRedirectServer _httpRedirectServer;

};
