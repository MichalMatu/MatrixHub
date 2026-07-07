#include <unity.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include "freertos/semphr.h"
#include "usb_terminal/output/TerminalOutput.h"
#include "usb_terminal/runtime/UsbTerminalCapturePump.h"
#include "usb_terminal/runtime/UsbTerminalSessionState.h"

#include "../../src/usb_terminal/output/AnsiFilter.cpp"
#include "../../src/usb_terminal/output/TerminalOutput.cpp"
#include "../../src/usb_terminal/runtime/UsbTerminalCapturePump.cpp"

namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
        (void)level;
        va_list args;
        va_start(args, fmt);
        printf("[%s] ", tag);
        vprintf(fmt, args);
        printf("\n");
        va_end(args);
    }
}

namespace {

SemaphoreHandle_t g_mutex = nullptr;

void freeResultText(USB_TERMINAL::CapturePumpResult& result) {
    if (result.eventText) {
        heap_caps_free(result.eventText);
        result.eventText = nullptr;
    }
}

}  // namespace

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 0;
    g_mutex = xSemaphoreCreateMutex();
}

void tearDown(void) {
    if (g_mutex) {
        vSemaphoreDelete(g_mutex);
        g_mutex = nullptr;
    }
}

void test_capture_pump_preserves_bytes_after_partial_flush() {
    USB_TERMINAL::TerminalOutput output;
    USB_TERMINAL::UsbTerminalSessionState session;
    USB_TERMINAL::UsbTerminalCapturePump pump;

    TEST_ASSERT_TRUE(output.begin());
    output.startCapture("ws:1");
    session.setActive(USB_TERMINAL::SessionTransport::Web, "ws:1");

    const size_t threshold = LIMITS::USB_TERMINAL::PARTIAL_FLUSH_THRESHOLD_BYTES;
    std::string prefix(threshold - 3, 'A');
    USB_TERMINAL::CapturePumpResult result = pump.process(
        g_mutex,
        output,
        session,
        false,
        2000,
        prefix.c_str(),
        prefix.size());
    TEST_ASSERT_NULL(result.eventText);

    const char* suffixTrigger = "0123456789";
    result = pump.process(
        g_mutex,
        output,
        session,
        false,
        2000,
        suffixTrigger,
        strlen(suffixTrigger));
    TEST_ASSERT_NOT_NULL(result.eventText);
    TEST_ASSERT_EQUAL(USB_TERMINAL::OutputPhase::Partial, result.eventPhase);
    TEST_ASSERT_EQUAL(threshold, strlen(result.eventText));
    TEST_ASSERT_EQUAL_STRING("ws:1", result.eventTargetId);
    TEST_ASSERT_EQUAL('0', result.eventText[threshold - 3]);
    TEST_ASSERT_EQUAL('1', result.eventText[threshold - 2]);
    TEST_ASSERT_EQUAL('2', result.eventText[threshold - 1]);
    freeResultText(result);

    result = pump.process(g_mutex, output, session, false, 2000, nullptr, 0);
    TEST_ASSERT_NULL(result.eventText);

    TEST_STUBS::ARDUINO::millisValue = 3000;
    result = pump.process(g_mutex, output, session, false, 2000, nullptr, 0);
    TEST_ASSERT_NOT_NULL(result.eventText);
    TEST_ASSERT_EQUAL(USB_TERMINAL::OutputPhase::Final, result.eventPhase);
    TEST_ASSERT_EQUAL_STRING("3456789", result.eventText);
    TEST_ASSERT_TRUE(result.shouldDispatchSessionChange);
    TEST_ASSERT_FALSE(result.sessionSnapshot.busy);
    freeResultText(result);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_capture_pump_preserves_bytes_after_partial_flush);
    return UNITY_END();
}
