#ifdef NATIVE_BUILD

#include <unity.h>

#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <vector>

#ifndef tskIDLE_PRIORITY
#define tskIDLE_PRIORITY 0
#endif

#include "../../src/system/button/ButtonHandler.cpp"

namespace {

int s_gpioLevel = HIGH;
bool s_menuActive = false;
bool s_menuEnabled = true;
uint32_t s_activityCount = 0;
uint32_t s_menuNextCount = 0;
uint32_t s_menuEnterCount = 0;
uint32_t s_menuSelectCount = 0;
uint32_t s_factoryResetCount = 0;
uint32_t s_resetWarningCount = 0;
uint32_t s_resetArmedCount = 0;
uint32_t s_resetCancelledCount = 0;
std::vector<uint8_t> s_clicks;

void setPhysicalPressed(bool pressed) {
    s_gpioLevel = HW::USER_BUTTON_ACTIVE_LOW ? (pressed ? LOW : HIGH) : (pressed ? HIGH : LOW);
}

void advance(ButtonHandler& button, uint32_t ms) {
    TEST_STUBS::ARDUINO::millisValue += ms;
    button.update();
}

void press(ButtonHandler& button) {
    setPhysicalPressed(true);
    button.update();
    advance(button, FACTORY::SHORT_PRESS_DEBOUNCE_MS);
}

void release(ButtonHandler& button) {
    setPhysicalPressed(false);
    button.update();
    advance(button, FACTORY::SHORT_PRESS_DEBOUNCE_MS);
}

void configureButton(ButtonHandler& button) {
    button.begin();

    ButtonHandler::Bindings bindings;
    bindings.onActivity = []() { ++s_activityCount; };
    bindings.isMenuActive = []() { return s_menuActive; };
    bindings.isMenuEnabled = []() { return s_menuEnabled; };
    bindings.onMenuNext = []() { ++s_menuNextCount; };
    bindings.onMenuEnter = []() { ++s_menuEnterCount; };
    bindings.onMenuSelect = []() { ++s_menuSelectCount; };
    bindings.onResetWarning = []() { ++s_resetWarningCount; };
    bindings.onResetArmed = []() { ++s_resetArmedCount; };
    bindings.onResetCancelled = []() { ++s_resetCancelledCount; };
    bindings.onFactoryReset = []() { ++s_factoryResetCount; };
    bindings.onMultiClick = [](uint8_t clickCount) { s_clicks.push_back(clickCount); };
    button.setBindings(std::move(bindings));
    button.setDoubleClickWindowMs(LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS + 50);
}

}  // namespace

extern "C" void pinMode(uint8_t pin, uint8_t mode) {
    (void)pin;
    (void)mode;
}

extern "C" void digitalWrite(uint8_t pin, uint8_t val) {
    (void)pin;
    (void)val;
}

extern "C" int digitalRead(uint8_t pin) {
    (void)pin;
    return s_gpioLevel;
}

extern "C" void delayMicroseconds(uint32_t us) {
    (void)us;
}

extern "C" int gpio_get_level(gpio_num_t gpio_num) {
    (void)gpio_num;
    return s_gpioLevel;
}

namespace LOG {

Settings Logging::_settings{ESP_LOG_VERBOSE};

void Logging::begin(const Settings& settings) {
    _settings = settings;
}

void Logging::setLevel(esp_log_level_t level) {
    _settings.level = level;
}

Settings Logging::settings() {
    return _settings;
}

bool Logging::isEnabled(esp_log_level_t level) {
    return level <= _settings.level;
}

void Logging::log(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    (void)level;
    (void)tag;
    (void)fmt;
}

void Logging::clearBuffer() {}

const char* Logging::levelToString(esp_log_level_t level) {
    (void)level;
    return "info";
}

esp_log_level_t Logging::stringToLevel(std::string_view name, esp_log_level_t fallback) {
    (void)name;
    return fallback;
}

void Logging::logStackHwm(const char* taskName, uint32_t stackSize) {
    (void)taskName;
    (void)stackSize;
}

void Logging::logSection(const char* title) {
    (void)title;
}

void Logging::suppressNoisyModules() {}

}  // namespace LOG

void setUp(void) {
    TEST_STUBS::ARDUINO::millisValue = 0;
    setPhysicalPressed(false);
    s_menuActive = false;
    s_menuEnabled = true;
    s_activityCount = 0;
    s_menuNextCount = 0;
    s_menuEnterCount = 0;
    s_menuSelectCount = 0;
    s_factoryResetCount = 0;
    s_resetWarningCount = 0;
    s_resetArmedCount = 0;
    s_resetCancelledCount = 0;
    s_clicks.clear();
}

void tearDown(void) {}

void test_short_press_dispatches_single_click_when_menu_inactive() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, 50);
    release(button);

    TEST_ASSERT_EQUAL_UINT32(1, s_activityCount);
    TEST_ASSERT_TRUE(s_clicks.empty());

    advance(button, LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS + 51);
    TEST_ASSERT_EQUAL_UINT32(1, s_clicks.size());
    TEST_ASSERT_EQUAL_UINT8(1, s_clicks[0]);
    TEST_ASSERT_EQUAL_UINT32(0, s_menuNextCount);
}

void test_short_press_navigates_menu_without_multi_click_when_menu_active() {
    ButtonHandler button;
    configureButton(button);
    s_menuActive = true;

    press(button);
    advance(button, 50);
    release(button);
    advance(button, LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS + 51);

    TEST_ASSERT_EQUAL_UINT32(1, s_menuNextCount);
    TEST_ASSERT_TRUE(s_clicks.empty());
}

void test_multi_click_accumulates_and_caps_at_three() {
    ButtonHandler button;
    configureButton(button);

    for (int i = 0; i < 4; ++i) {
        press(button);
        advance(button, 30);
        release(button);
        advance(button, 50);
    }

    advance(button, LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS + 51);

    TEST_ASSERT_EQUAL_UINT32(1, s_clicks.size());
    TEST_ASSERT_EQUAL_UINT8(3, s_clicks[0]);
}

// New behavior: menu enter must fire *while still held* at the 2s threshold,
// so the user does not have to release the button to open the menu.
void test_medium_hold_enters_menu_immediately_while_held() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    TEST_ASSERT_EQUAL_UINT32(0, s_menuEnterCount);

    advance(button, FACTORY::MEDIUM_PRESS_MS + 20);
    TEST_ASSERT_EQUAL_UINT32(1, s_menuEnterCount);

    // Release must not fire a duplicate menu action.
    release(button);
    TEST_ASSERT_EQUAL_UINT32(1, s_menuEnterCount);
    TEST_ASSERT_TRUE(s_clicks.empty());
}

void test_medium_hold_enters_menu_even_when_update_runs_after_reset_threshold() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::LONG_PRESS_MS + 20);

    TEST_ASSERT_EQUAL_UINT32(1, s_menuEnterCount);
    TEST_ASSERT_EQUAL_UINT32(1, s_resetWarningCount);
    TEST_ASSERT_EQUAL_UINT32(1, s_resetArmedCount);

    release(button);
    TEST_ASSERT_EQUAL_UINT32(1, s_menuEnterCount);
}

void test_medium_hold_selects_menu_item_immediately_when_active() {
    ButtonHandler button;
    configureButton(button);
    s_menuActive = true;

    press(button);
    advance(button, FACTORY::MEDIUM_PRESS_MS + 20);
    TEST_ASSERT_EQUAL_UINT32(1, s_menuSelectCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_menuNextCount);

    release(button);
    TEST_ASSERT_EQUAL_UINT32(1, s_menuSelectCount);
}

void test_medium_hold_does_not_enter_menu_when_disabled() {
    ButtonHandler button;
    configureButton(button);
    s_menuEnabled = false;

    press(button);
    advance(button, FACTORY::MEDIUM_PRESS_MS + 20);
    release(button);

    TEST_ASSERT_EQUAL_UINT32(0, s_menuEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_menuSelectCount);
    TEST_ASSERT_TRUE(s_clicks.empty());
}

void test_press_between_short_and_medium_threshold_is_noop() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::SHORT_PRESS_MAX_MS + 100);
    release(button);
    advance(button, LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS + 51);

    TEST_ASSERT_EQUAL_UINT32(1, s_activityCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_menuEnterCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_menuSelectCount);
    TEST_ASSERT_TRUE(s_clicks.empty());
}

// At 7 s the warning hook fires so the matrix can show a heads-up. The actual
// reset must NOT have triggered yet.
void test_reset_warning_fires_at_7s_without_resetting() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::RESET_WARNING_MS - 100);
    TEST_ASSERT_EQUAL_UINT32(0, s_resetWarningCount);

    advance(button, 200);
    TEST_ASSERT_EQUAL_UINT32(1, s_resetWarningCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_resetArmedCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_factoryResetCount);

    release(button);
}

// At 10 s the reset becomes ARMED but is NOT executed. The user must release
// and double-click to actually wipe config.
void test_reset_armed_at_10s_does_not_execute_until_confirmed() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::LONG_PRESS_MS + 50);

    TEST_ASSERT_EQUAL_UINT32(1, s_resetWarningCount);
    TEST_ASSERT_EQUAL_UINT32(1, s_resetArmedCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_factoryResetCount);

    // Still nothing on release alone.
    release(button);
    TEST_ASSERT_EQUAL_UINT32(0, s_factoryResetCount);
}

// Two presses within the confirmation window trigger the actual reset.
void test_double_click_within_window_confirms_factory_reset() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::LONG_PRESS_MS + 50);
    release(button);
    TEST_ASSERT_EQUAL_UINT32(0, s_factoryResetCount);

    // Two quick clicks inside the window.
    advance(button, 50);
    press(button);
    advance(button, 30);
    release(button);
    TEST_ASSERT_EQUAL_UINT32(0, s_factoryResetCount);

    advance(button, 50);
    press(button);
    advance(button, 30);
    release(button);
    TEST_ASSERT_EQUAL_UINT32(1, s_factoryResetCount);
    TEST_ASSERT_EQUAL_UINT32(0, s_resetCancelledCount);
}

// If no confirmation arrives, the armed state expires gracefully.
void test_armed_state_times_out_into_cancellation() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::LONG_PRESS_MS + 50);
    release(button);

    advance(button, FACTORY::RESET_CONFIRM_WINDOW_MS + 100);
    TEST_ASSERT_EQUAL_UINT32(0, s_factoryResetCount);
    TEST_ASSERT_EQUAL_UINT32(1, s_resetCancelledCount);
}

// During the confirmation window the click presses must NOT also fire as
// AirMouse multi-clicks.
void test_confirmation_clicks_are_not_dispatched_to_multi_click() {
    ButtonHandler button;
    configureButton(button);

    press(button);
    advance(button, FACTORY::LONG_PRESS_MS + 50);
    release(button);

    advance(button, 50);
    press(button);
    advance(button, 30);
    release(button);
    advance(button, 50);
    press(button);
    advance(button, 30);
    release(button);

    advance(button, LIMITS::AIR_MOUSE::MIN_DOUBLE_CLICK_MS + 200);
    TEST_ASSERT_TRUE(s_clicks.empty());
    TEST_ASSERT_EQUAL_UINT32(1, s_factoryResetCount);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_short_press_dispatches_single_click_when_menu_inactive);
    RUN_TEST(test_short_press_navigates_menu_without_multi_click_when_menu_active);
    RUN_TEST(test_multi_click_accumulates_and_caps_at_three);
    RUN_TEST(test_medium_hold_enters_menu_immediately_while_held);
    RUN_TEST(test_medium_hold_enters_menu_even_when_update_runs_after_reset_threshold);
    RUN_TEST(test_medium_hold_selects_menu_item_immediately_when_active);
    RUN_TEST(test_medium_hold_does_not_enter_menu_when_disabled);
    RUN_TEST(test_press_between_short_and_medium_threshold_is_noop);
    RUN_TEST(test_reset_warning_fires_at_7s_without_resetting);
    RUN_TEST(test_reset_armed_at_10s_does_not_execute_until_confirmed);
    RUN_TEST(test_double_click_within_window_confirms_factory_reset);
    RUN_TEST(test_armed_state_times_out_into_cancellation);
    RUN_TEST(test_confirmation_clicks_are_not_dispatched_to_multi_click);
    return UNITY_END();
}

#endif  // NATIVE_BUILD
