/**
 * @file LogOutput.h
 * @brief USB Serial log output management
 * 
 * Manages the TinyUSB CDC serial port for log output.
 * Provides:
 *  - USB stack initialization (USB.begin + USBSerial)
 *  - Redirection of esp_log to USBSerial
 *  - Boot log replay from RingBuffer to USBSerial
 */
#pragma once

namespace LOG {
namespace Output {

/**
 * @brief Initialize USB stack and redirect ESP-IDF logs to USBSerial
 * 
 * Call sequence:
 *  1. USB.begin() — start TinyUSB composite device
 *  2. USBSerial.begin() — initialize CDC serial
 *  3. Redirect esp_log via custom_vprintf
 *  4. Wait 500ms for host re-enumeration
 *  5. Dump buffered boot logs
 */
void beginUsb();

}  // namespace Output
}  // namespace LOG
