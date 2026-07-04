#include <core/ESP32SvelteKit.h>
#include <PsychicHttpsServer.h>
#include "config/App.h"
#include "system/Application.h"
#include "system/logging/Logging.h"

using namespace APP;

PsychicHttpsServer server;
ESP32SvelteKit esp32sveltekit(&server, CONFIG::Keys::FRAMEWORK::MAX_URI_HANDLERS);

#undef LOG_TAG
#define LOG_TAG "Main"

#ifndef UNIT_TEST
void setup() {
    LOG::Logging::begin(LOG::Settings{LOGGING::STARTUP_LEVEL});
    Application::instance().setup(server, esp32sveltekit);
}

void loop() {
    Application::instance().loop();
}
#endif
