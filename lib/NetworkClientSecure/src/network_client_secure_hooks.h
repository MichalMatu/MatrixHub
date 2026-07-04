#pragma once

#include <Arduino.h>

extern "C" void network_client_secure_wait_hook();

inline void network_client_secure_feed_watchdog() {
  network_client_secure_wait_hook();
}

inline void network_client_secure_yield_wait(uint32_t delay_ms = 1) {
  network_client_secure_wait_hook();
  if (delay_ms > 0) {
    delay(delay_ms);
  }
}
