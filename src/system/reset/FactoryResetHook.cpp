#include "FactoryReset.h"
#include "../Application.h"

// Application hook used by the embedded web/API framework runtime.
// When the UI calls POST /rest/factoryReset, the framework FactoryResetService
// will delegate here (if present) instead of only deleting config files.
// NOTE: Application::instance() is used here because this is an extern "C"
// framework callback — an entry point equivalent to main.cpp.
// TODO(next cleanup pass): If the framework ever allows registering this hook
// with user context or a bound service object, pass ServiceRegistry directly
// and remove the Application::instance() reach-through here. Keeping it local
// to this entry point is acceptable for now; the important boundary is that the
// rest of runtime code should keep using explicit DI instead of reviving the
// singleton/service-locator pattern.
extern "C" void svk_appFactoryReset() {
    auto* registry = Application::instance().getServiceRegistry();
    if (registry) {
        SYSTEM::performFactoryReset(*registry);
    }
}
