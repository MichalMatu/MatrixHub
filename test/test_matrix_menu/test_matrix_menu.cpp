/**
 * @file test_matrix_menu.cpp
 * @brief Unit tests for MatrixMenuService behavior
 */

#ifdef UNIT_TEST

#include <unity.h>
#include <Arduino.h>
#include <WiFi.h>

#include "../../src/system/rtc/RtcConfig.h"
#include "../../src/sensors/runtime/SensorState.h"
#include "../../src/system/logging/Logging.h"
#include "../../src/system/matrix_manager/MatrixManagerService.h"

// ---------------------------------------------------------------------------
// RTC stubs (avoid linking full RtcConfig.cpp)
// ---------------------------------------------------------------------------
namespace RTC {
    ConfigStore mockStore;

    ConfigStore& getMutableConfig() {
        return mockStore;
    }

    const ConfigStore& getConfig() {
        return mockStore;
    }

    void withConfig(const std::function<void(const ConfigStore&)>& reader) {
        reader(mockStore);
    }
}

// ---------------------------------------------------------------------------
// SensorState stub (avoid linking sensor runtime)
// ---------------------------------------------------------------------------
namespace SENSORS {
    SemaphoreHandle_t SensorState::_initMutex = nullptr;
    SemaphoreHandle_t SensorState::_snapshotMutex = nullptr;
    SensorSnapshot SensorState::_latestSnapshot{};
    SensorSnapshot SensorState::_lastGoodSnapshot{};
    PhaseStatus SensorState::_lastReadStatus{};
    PhaseStatus SensorState::_lastWriteStatus{};
    ErrorInfo SensorState::_lastErrorInfo{};
    std::atomic<bool> SensorState::_initialized{false};

    SensorSnapshot SensorState::getSnapshot() {
        SensorSnapshot snap{};
        snap.timestamp_ms = 0;
        snap.temp = 0.0f;
        snap.humid = 0.0f;
        snap.co2 = 0;
        return snap;
    }
}

// ---------------------------------------------------------------------------
// Logging stubs
// ---------------------------------------------------------------------------
namespace LOG {
    Settings Logging::_settings;
    void Logging::log(esp_log_level_t level, const char* tag, const char* format, ...) {}
    void Logging::logSection(const char* title) {}
    void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {}
}

// ---------------------------------------------------------------------------
// Time control for MatrixMenuService.cpp
// ---------------------------------------------------------------------------
static unsigned long _mockMillis = 0;
#define millis() _mockMillis
#include "../../src/matrix/menu/MatrixMenuService.cpp"
#undef millis

// ---------------------------------------------------------------------------
// MatrixManagerService stubs (linker)
// ---------------------------------------------------------------------------
namespace MATRIX_MANAGER {
    static uint32_t g_setLayerCalls = 0;
    static uint32_t g_clearLayerCalls = 0;
    static Layer g_lastSetLayer = Layer::BACKGROUND;
    static Layer g_lastClearLayer = Layer::BACKGROUND;

    void MatrixManagerService::setLayer(Layer layer, const LayerContent& content) {
        g_setLayerCalls++;
        g_lastSetLayer = layer;
        (void)content;
    }

    void MatrixManagerService::clearLayer(Layer layer) {
        g_clearLayerCalls++;
        g_lastClearLayer = layer;
    }
}

// WiFi global from stub
WiFiClass WiFi;

using namespace MATRIX;

static MatrixService g_matrix;
static WifiMenuAction g_currentWifiMode = WifiMenuAction::Station;
static WifiMenuAction g_lastRequestedWifiMode = WifiMenuAction::Station;
static uint32_t g_wifiModeRequestCalls = 0;
static bool g_wifiModeRequestResult = true;

void setUp(void) {
    memset(&RTC::mockStore, 0, sizeof(RTC::mockStore));
    RTC::mockStore.matrix.menu.enabled = true;
    RTC::mockStore.matrix.menu.textColor = 0xFFFFFF;
    RTC::mockStore.matrix.menu.scrollSpeed = 20;
    _mockMillis = 0;

    g_matrix.showTextCalls = 0;
    g_matrix.clearCalls = 0;
    MATRIX_MANAGER::g_setLayerCalls = 0;
    MATRIX_MANAGER::g_clearLayerCalls = 0;
    MATRIX_MANAGER::g_lastSetLayer = MATRIX_MANAGER::Layer::BACKGROUND;
    MATRIX_MANAGER::g_lastClearLayer = MATRIX_MANAGER::Layer::BACKGROUND;
    g_currentWifiMode = WifiMenuAction::Station;
    g_lastRequestedWifiMode = WifiMenuAction::Station;
    g_wifiModeRequestCalls = 0;
    g_wifiModeRequestResult = true;

}

void tearDown(void) {}

void test_menu_cycles_screens() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.nextScreen();
    TEST_ASSERT_TRUE(menu.isActive());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::TIME),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::SENSORS),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::IP),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_STATION),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_ACCESS_POINT),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_DISABLED),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::EXIT),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::TIME),
                            static_cast<uint8_t>(menu.current()));
}

void test_enter_menu_opens_time_screen_without_advancing_active_menu() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.enterMenu();
    TEST_ASSERT_TRUE(menu.isActive());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::TIME),
                            static_cast<uint8_t>(menu.current()));

    menu.enterMenu();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::TIME),
                            static_cast<uint8_t>(menu.current()));
}

void test_menu_disables_and_exits() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.nextScreen();
    TEST_ASSERT_TRUE(menu.isActive());

    RTC::mockStore.matrix.menu.enabled = false;
    menu.update();

    TEST_ASSERT_FALSE(menu.isActive());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::NONE),
                            static_cast<uint8_t>(menu.current()));
    TEST_ASSERT_TRUE(g_matrix.clearCalls > 0);
}

void test_exit_menu_clears_menu_layer_when_manager_present() {
    alignas(MATRIX_MANAGER::MatrixManagerService) unsigned char managerStorage[sizeof(MATRIX_MANAGER::MatrixManagerService)] = {};
    auto* manager = reinterpret_cast<MATRIX_MANAGER::MatrixManagerService*>(managerStorage);

    MatrixMenuService menu(&g_matrix, manager);
    menu.enterMenu();
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_setLayerCalls);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX_MANAGER::Layer::MENU),
                            static_cast<uint8_t>(MATRIX_MANAGER::g_lastSetLayer));

    menu.exitMenu();
    TEST_ASSERT_EQUAL_UINT32(1, MATRIX_MANAGER::g_clearLayerCalls);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MATRIX_MANAGER::Layer::MENU),
                            static_cast<uint8_t>(MATRIX_MANAGER::g_lastClearLayer));
}

void test_exit_screen_select_closes_menu() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.enterMenu();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::EXIT),
                            static_cast<uint8_t>(menu.current()));

    menu.selectCurrent();

    TEST_ASSERT_FALSE(menu.isActive());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::NONE),
                            static_cast<uint8_t>(menu.current()));
    TEST_ASSERT_TRUE(g_matrix.clearCalls > 0);
}

void test_wifi_action_requires_two_click_confirmation_then_requests_restart() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.setWifiModeActions(
        []() { return g_currentWifiMode; },
        [](WifiMenuAction action) {
            g_wifiModeRequestCalls++;
            g_lastRequestedWifiMode = action;
            return g_wifiModeRequestResult;
        });

    menu.enterMenu();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_ACCESS_POINT),
                            static_cast<uint8_t>(menu.current()));

    menu.selectCurrent();
    TEST_ASSERT_EQUAL_STRING("2x OK", g_matrix.lastText);
    TEST_ASSERT_EQUAL_UINT32(0, g_wifiModeRequestCalls);

    menu.nextScreen();
    TEST_ASSERT_EQUAL_STRING("1/2", g_matrix.lastText);
    TEST_ASSERT_EQUAL_UINT32(0, g_wifiModeRequestCalls);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_ACCESS_POINT),
                            static_cast<uint8_t>(menu.current()));

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT32(1, g_wifiModeRequestCalls);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(WifiMenuAction::AccessPoint),
                            static_cast<uint8_t>(g_lastRequestedWifiMode));
    TEST_ASSERT_EQUAL_STRING("RESTART", g_matrix.lastText);
}

void test_wifi_confirmation_timeout_cancels_without_request() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.setWifiModeActions(
        []() { return g_currentWifiMode; },
        [](WifiMenuAction action) {
            g_wifiModeRequestCalls++;
            g_lastRequestedWifiMode = action;
            return true;
        });

    menu.enterMenu();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    menu.selectCurrent();

    _mockMillis = FACTORY::RESET_CONFIRM_WINDOW_MS + 10;
    menu.update();

    TEST_ASSERT_EQUAL_UINT32(0, g_wifiModeRequestCalls);
    TEST_ASSERT_EQUAL_STRING("CANCEL", g_matrix.lastText);

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_DISABLED),
                            static_cast<uint8_t>(menu.current()));
}

void test_selecting_current_wifi_mode_does_not_request_restart() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.setWifiModeActions(
        []() { return g_currentWifiMode; },
        [](WifiMenuAction action) {
            g_wifiModeRequestCalls++;
            g_lastRequestedWifiMode = action;
            return true;
        });

    menu.enterMenu();
    menu.nextScreen();
    menu.nextScreen();
    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_STATION),
                            static_cast<uint8_t>(menu.current()));

    menu.selectCurrent();

    TEST_ASSERT_EQUAL_UINT32(0, g_wifiModeRequestCalls);
    TEST_ASSERT_EQUAL_STRING("STA ACTIVE", g_matrix.lastText);

    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::WIFI_ACCESS_POINT),
                            static_cast<uint8_t>(menu.current()));
}

void test_invalidate_cache_forces_rerender() {
    MatrixMenuService menu(&g_matrix, nullptr);
    menu.nextScreen();
    TEST_ASSERT_EQUAL_UINT32(1, g_matrix.showTextCalls);

    // First update syncs internal timers/state
    _mockMillis = 10;
    menu.update();
    TEST_ASSERT_EQUAL_UINT32(1, g_matrix.showTextCalls);

    // Cache invalidation should force a new render even if refresh window not reached
    _mockMillis = 100;
    menu.invalidateCache();
    menu.update();
    TEST_ASSERT_EQUAL_UINT32(2, g_matrix.showTextCalls);
}

void test_ip_screen_shows_short_addresses_without_https_prefix() {
    TEST_STUBS::WIFI::mode = WIFI_STA;
    TEST_STUBS::WIFI::status = WL_CONNECTED;
    TEST_STUBS::WIFI::hostname = "matrixhub";
    TEST_STUBS::WIFI::localIp = 0xC0A80012;

    MatrixMenuService menu(&g_matrix, nullptr);
    menu.enterMenu();
    menu.nextScreen();
    menu.nextScreen();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(RTC::MatrixMenuScreen::IP),
                            static_cast<uint8_t>(menu.current()));
    TEST_ASSERT_EQUAL_STRING("matrixhub.local 192.168.0.18", g_matrix.lastText);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_menu_cycles_screens);
    RUN_TEST(test_enter_menu_opens_time_screen_without_advancing_active_menu);
    RUN_TEST(test_menu_disables_and_exits);
    RUN_TEST(test_exit_menu_clears_menu_layer_when_manager_present);
    RUN_TEST(test_exit_screen_select_closes_menu);
    RUN_TEST(test_wifi_action_requires_two_click_confirmation_then_requests_restart);
    RUN_TEST(test_wifi_confirmation_timeout_cancels_without_request);
    RUN_TEST(test_selecting_current_wifi_mode_does_not_request_restart);
    RUN_TEST(test_invalidate_cache_forces_rerender);
    RUN_TEST(test_ip_screen_shows_short_addresses_without_https_prefix);
    return UNITY_END();
}

#endif
