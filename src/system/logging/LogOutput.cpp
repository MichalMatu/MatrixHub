/**
 * @file LogOutput.cpp
 * @brief USB Serial log output without ANSI colors (saves stack and prevents garbage chars)
 */
#include "LogOutput.h"
#include "Logging.h"

#include <USB.h>
#include <USBCDC.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cstdio>
#include <cstdarg>

USBCDC USBSerial;

namespace LOG {
namespace Output {

namespace {

bool writeBootLogLine(const Line& line) {
    char buf[LOG_CFG::USB_BOOT_LINE_BUF_BYTES];
    int len = snprintf(buf, sizeof(buf), "%c (%lu) [%s] %s\n",
                       line.levelChar, (unsigned long)line.timestampMs,
                       line.tag, line.message);
    if (len <= 0) {
        return true;
    }

    const size_t writeLen = (len < (int)sizeof(buf)) ? (size_t)len : sizeof(buf) - 1;
    if (USBSerial.availableForWrite() < writeLen) {
        return false;
    }

    USBSerial.write((const uint8_t*)buf, writeLen);
    return true;
}

}  // namespace

// ─── Custom vprintf for ESP-IDF log redirection ───────────────────

static int usbVprintf(const char* fmt, va_list ap) {
    if (!USBSerial) return 0; // Fast reject if host is not connected

    // Protection against ESP-IDF Kernel Panic when writing from an ISR (Interrupt Service Routine) context
    if (xPortInIsrContext()) {
        return 0; 
    }

    // Allocated on task stack to avoid race conditions (especially from ISRs)
    char buf[LOG_CFG::USB_VPRINTF_BUF_BYTES];
    int ret = vsnprintf(buf, sizeof(buf), fmt, ap);
    
    if (ret > 0) {
        size_t writeLen = (ret < (int)sizeof(buf)) ? (size_t)ret : sizeof(buf) - 1;
        
        // Safety check in case the Serial Monitor is disabled
        // or the USB host cannot keep up with receiving (protection against Watchdog stall)
        if (USBSerial.availableForWrite() >= writeLen) {
             USBSerial.write((const uint8_t*)buf, writeLen);
        }
    }
    return ret;
}

// ─── Boot Log Dump ────────────────────────────────────────────────

static void dumpBootLogs() {
    if (!USBSerial || USBSerial.availableForWrite() == 0) {
        return;
    }

    Line* bootLines = (Line*)heap_caps_malloc(
        sizeof(Line) * LOG_CFG::USB_BOOT_REPLAY_CHUNK_LINES,
        MALLOC_CAP_SPIRAM);
    if (!bootLines) {
        return;
    }

    bool outputSaturated = false;
    size_t offset = 0;
    while (!outputSaturated) {
        const size_t copied = RingBuffer::copyTailRange(
            bootLines,
            LOG_CFG::USB_BOOT_REPLAY_MAX_LINES,
            offset,
            LOG_CFG::USB_BOOT_REPLAY_CHUNK_LINES);
        if (copied == 0) {
            break;
        }

        for (size_t i = 0; i < copied; i++) {
            const Line& line = bootLines[i];
            outputSaturated = !writeBootLogLine(line);
            if (outputSaturated) {
                break;
            }
        }

        offset += copied;
    }

    heap_caps_free(bootLines);
}

// ─── Public API ───────────────────────────────────────────────────

void beginUsb() {
    USB.begin();
    USBSerial.begin();
    
    // Redirect to new function without colors
    esp_log_set_vprintf(usbVprintf);

    // Allow time for host USB re-enumeration
    vTaskDelay(pdMS_TO_TICKS(LOG_CFG::USB_ENUM_DELAY_MS));

    // Replay buffered boot logs to the new TinyUSB port
    dumpBootLogs();
}

}  // namespace Output
}  // namespace LOG
