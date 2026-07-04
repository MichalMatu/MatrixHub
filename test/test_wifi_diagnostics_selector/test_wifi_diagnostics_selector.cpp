#include <unity.h>

#include "../../lib/framework/wifi/WifiDiagnosticsSelector.h"

#include <vector>

namespace {

struct TestNetwork {
    const char* ssid;
};

}  // namespace

using namespace WIFI_DIAGNOSTICS;

void test_returns_no_index_for_empty_network_list() {
    const std::vector<TestNetwork> networks;

    const size_t index = selectNetworkIndex(
        networks,
        "Office",
        true,
        0,
        [](const TestNetwork& network) { return network.ssid; });

    TEST_ASSERT_EQUAL_UINT32(kNoNetworkIndex, index);
}

void test_prefers_connected_ssid_match_over_primary_network() {
    const std::vector<TestNetwork> networks = {
        {"Home"},
        {"Office"},
        {"Backup"},
    };

    const size_t index = selectNetworkIndex(
        networks,
        "Office",
        true,
        0,
        [](const TestNetwork& network) { return network.ssid; });

    TEST_ASSERT_EQUAL_UINT32(1, index);
}

void test_falls_back_to_current_network_index_when_sta_is_not_connected() {
    const std::vector<TestNetwork> networks = {
        {"Home"},
        {"Office"},
        {"Backup"},
    };

    const size_t index = selectNetworkIndex(
        networks,
        nullptr,
        false,
        2,
        [](const TestNetwork& network) { return network.ssid; });

    TEST_ASSERT_EQUAL_UINT32(2, index);
}

void test_falls_back_to_primary_network_when_no_match_and_index_invalid() {
    const std::vector<TestNetwork> networks = {
        {"Home"},
        {"Office"},
    };

    const size_t index = selectNetworkIndex(
        networks,
        "Unknown",
        true,
        99,
        [](const TestNetwork& network) { return network.ssid; });

    TEST_ASSERT_EQUAL_UINT32(0, index);
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_returns_no_index_for_empty_network_list);
    RUN_TEST(test_prefers_connected_ssid_match_over_primary_network);
    RUN_TEST(test_falls_back_to_current_network_index_when_sta_is_not_connected);
    RUN_TEST(test_falls_back_to_primary_network_when_no_match_and_index_invalid);
    return UNITY_END();
}
