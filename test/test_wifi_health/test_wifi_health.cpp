#include <unity.h>
#include <cstring>
#include "../../src/system/health/wifi/WifiHealthMonitor.h"
#include "../../src/system/health/wifi/WifiHealthMonitor.cpp" // For native linking

using namespace SYSTEM::HEALTH;

WifiHealthMonitor* monitor;

void setUp(void) {
    monitor = new WifiHealthMonitor();
}

void tearDown(void) {
    delete monitor;
}

// ============================================================================
// Test Cases
// ============================================================================

void test_initial_state() {
    TEST_ASSERT_FALSE(monitor->getHealth().isConnected);
    TEST_ASSERT_EQUAL_UINT32(0, monitor->getHealth().bootTimeMs);
    TEST_ASSERT_EQUAL_UINT32(0, monitor->getHealth().reconnectCount);
}

void test_connection_flow() {
    uint32_t t = 1000;
    monitor->onConnected("TestSSID", -60, t);
    
    TEST_ASSERT_TRUE(monitor->getHealth().isConnected);
    TEST_ASSERT_EQUAL_STRING("TestSSID", monitor->getHealth().lastSsid);
    TEST_ASSERT_EQUAL_INT8(-60, monitor->getHealth().currentRssi);
    TEST_ASSERT_EQUAL_UINT32(t, monitor->getHealth().lastConnectMs);
    
    // Disconnect
    t += 5000;
    monitor->onDisconnected(5, t); // Reason 5
    
    TEST_ASSERT_FALSE(monitor->getHealth().isConnected);
    TEST_ASSERT_EQUAL_UINT32(t, monitor->getHealth().lastDisconnectMs);
    TEST_ASSERT_EQUAL_UINT8(5, monitor->getHealth().lastDisconnectReason);
    TEST_ASSERT_EQUAL_UINT16(1, monitor->getHealth().reconnectCount);
}

void test_health_check_rssi() {
    monitor->onConnected("TestSSID", -50, 1000);
    TEST_ASSERT_TRUE(monitor->isHealthy(5000));
    
    // Degrade signal
    monitor->updateRssi(-90); // Below -85 threshold
    TEST_ASSERT_FALSE(monitor->isHealthy(5000));
    
    // Restore signal
    monitor->updateRssi(-70);
    TEST_ASSERT_TRUE(monitor->isHealthy(5000));
}

void test_health_check_reconnect_rate() {
    monitor->onConnected("TestSSID", -50, 1000);
    
    // Simulate 1 hour uptime
    uint32_t uptime = 3600000;
    
    // Trigger 5 reconnects (safe limit is 10/h)
    for (int i = 0; i < 5; i++) {
        monitor->onDisconnected(1, uptime);
        monitor->onConnected("TestSSID", -50, uptime + 100);
    }
    
    TEST_ASSERT_TRUE(monitor->isHealthy(uptime));
    
    // Trigger 6 more (total 11) -> Unhealthy
    for (int i = 0; i < 6; i++) {
        monitor->onDisconnected(1, uptime);
        monitor->onConnected("TestSSID", -50, uptime + 100);
    }
    
    TEST_ASSERT_FALSE(monitor->isHealthy(uptime));
}

void test_short_uptime_handling() {
    // If uptime is < 1h, it treats it as 1h for rate calc to avoid division by zero or huge rates
    monitor->onConnected("TestSSID", -50, 100);
    
    // 5 reconnects in 1 minute -> technically very bad, but our simple algorithm
    // divides by max(1, uptimeHours). So 5 / 1 = 5. Safe.
    // This prevents false positives on unstable boot.
    for (int i = 0; i < 5; i++) {
        monitor->onDisconnected(1, 100);
        monitor->onConnected("TestSSID", -50, 200);
    }
    
    TEST_ASSERT_TRUE(monitor->isHealthy(60000));
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    
    UNITY_BEGIN();
    
    RUN_TEST(test_initial_state);
    RUN_TEST(test_connection_flow);
    RUN_TEST(test_health_check_rssi);
    RUN_TEST(test_health_check_reconnect_rate);
    RUN_TEST(test_short_uptime_handling);
    
    return UNITY_END();
}
