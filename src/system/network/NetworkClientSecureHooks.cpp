#include "../watchdog/TaskWatchdog.h"

extern "C" void network_client_secure_wait_hook() {
    auto& watchdog = SYSTEM::TaskWatchdog::instance();
    if (watchdog.isInitialized()) {
        (void)watchdog.reset();
    }
}
