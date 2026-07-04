#include "PowerInitializer.h"

#include "../../../config/Hardware.h"
#include "../../../config/System.h"
#include "../../logging/Logging.h"
#include "../../power/PowerManager.h"
#include "../../services/ServiceRegistry.h"
#include "../../shutdown/ShutdownSequence.h"
#include "../../watchdog/TaskWatchdog.h"

#include <MatrixService.h>
#undef LOG_TAG
#define LOG_TAG "PowerInit"

namespace SYSTEM {

namespace {
// Trampoline for pre-sleep hook: bridges void(*)() with ShutdownSequence::execute(ServiceRegistry&).
// Safe because there is exactly one ServiceRegistry instance per boot.
ServiceRegistry* s_registryForSleepHook = nullptr;

void preSleepTrampoline() {
    if (s_registryForSleepHook) {
        ShutdownSequence::execute(*s_registryForSleepHook);
    }
}
}  // namespace

void PowerInitializer::initialize(ServiceRegistry& services) {
    LOGI("[Phase3] Starting PowerManager...");

    TaskWatchdog::instance().begin(WATCHDOG::TIMEOUT_SEC, BOOT::WATCHDOG::PANIC_ON_TIMEOUT);
    if (!TaskWatchdog::instance().isInitialized()) {
        // Keep the primary product path available even when TWDT init fails.
        // Runtime fault recovery uses direct esp_restart() paths and does not
        // depend on a graceful shutdown of the web stack.
        LOGW("[Phase3] Failed to initialize TaskWatchdog - continuing without hardware watchdog");
    }

    POWER::PowerManager* pm = services.getPowerManager();
    pm->begin();
    s_registryForSleepHook = &services;
    pm->setPreSleepHook(preSleepTrampoline);

    const auto wakeReason = pm->wakeReason();
    LOGI("[Phase3] Wake reason: %d", static_cast<int>(wakeReason));

    LOGI("[Phase3] Initializing MatrixService...");
    if (auto* matrix = services.getMatrixService()) {
        matrix->init(MATRIX::PIN);
    }

    LOGI("[Phase3] Power and LED/Matrix initialized");
}

}  // namespace SYSTEM
