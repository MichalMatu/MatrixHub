# Architecture Reference

## When To Read This File

- Read this file when the task touches memory layout, task pinning, ServiceRegistry wiring, HTTP or BLE design, CSI streaming, HTTPS, or repo structure.

## Memory Strategy

- Treat internal DRAM as scarce even with PSRAM available.
- Default to PSRAM for buffers that do not require DMA or ISR access.
- Use PSRAM for user-space buffers larger than about 512 bytes.
- Prefer static buffers plus placement new for permanent objects when it reduces heap fragmentation.
- Use internal DRAM for DMA buffers such as SPI, I2S, RMT, and matrix display buffers.
- Use internal DRAM for any variable, queue, or buffer accessed in an ISR.
- Small or ultra-low-latency tasks under about 4 KB stack can stay in internal DRAM.
- Use `SYSTEM::SpiRamJsonDocument` for larger JSON documents.
- Stream large API responses instead of building huge `String` objects.
- Always free `heap_caps_malloc()` allocations with `heap_caps_free()`.

## Task Pinning

- Task configs live in `CONFIG::TASKS` in `src/config/System.h`.
- Core 0 is for WiFi, BLE, ESP-NOW, and heavy networking.
- Core 1 is for `loop()`, UI logic, matrix work, and light system maintenance.
- Do not hardcode task stack, core, or priority inline.

## ServiceRegistry And Initialization

- Treat `ServiceRegistry` as the composition root and owner graph.
- Ownership lives in `src/system/services/ServiceRegistry.*`.
- API `PsramStaticService` storage lives in `src/system/services/ServiceRegistryApi.h`.
- Initialization is split across `src/system/init/*Initializer*.cpp`.
- Prefer constructor injection or explicit `init(...)` wiring.
- API services under `src/api/` should receive business-service pointers explicitly.
- Existing repo-wide `instance()` patterns are compatibility exceptions, not models for new features.

## Configuration Rules

- Put timeouts, intervals, thresholds, buffer sizes, and pin definitions in `src/config/System.h` or `src/config/Hardware.h`.
- User-changeable settings should serialize through `src/config/json/*.h` and `ConfigManager`.
- Preserve the RTC plus JSON persistence flow when touching config-backed features.

## HTTP Conventions

- Preserve existing route families. Most domain and config endpoints use `/api/...`, while framework or compatibility paths may use `/rest/...`.
- Prefer matching the surrounding feature instead of inventing a third naming scheme.
- Prefer `Response::success(...)` and `Response::error(...)` for small structured responses.
- Prefer `Utils::JsonResponseWriter` for larger or streamed JSON payloads.
- Use `request->reply(...)` only for simple status or plain responses, or when the existing feature already follows that pattern.
- Use PSRAM-backed temporary buffers for larger response-building paths.

## BLE And Security

- Enforce bonding with `BLE_SM_PAIR_AUTHREQ_MITM | BLE_SM_PAIR_AUTHREQ_BOND` when security matters.
- Use `READ_ENC` and `WRITE_ENC` for sensitive characteristics.
- For streamed lists such as files or logs, prefer vectors of POD structs or fixed-size records.
- Verify JWT expiration and user state on every authenticated request.

## CSI Guidelines

- Treat `wifi_csi_rx_cb` as ISR context.
- Never allocate memory, block, or take mutexes in the callback.
- Push fixed-size packets into a static queue and process them in a worker task.
- Keep raw CSI transport binary for high throughput.
- If the framework abstraction is insufficient for binary WebSocket streaming, register the handler manually and send frames asynchronously.

## HTTPS Guidelines

- Enable HTTPS via `CONFIG_ESP_HTTPS_SERVER_ENABLE`.
- Prefer external memory for SSL buffers via `CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC`.
- Limit HTTPS socket counts to protect RAM.
- Hardcoded certs currently live in `src/config/certificates.h`; preserve existing behavior unless the task explicitly changes certificate storage.
- Rate limiting should cap tracked client state and use memory-friendly containers.

## Directory Guide

- `src/api/`: HTTP controllers and API services.
- `src/ble/`: BLE facade and services.
- `src/config/`: repo-level constants and JSON config helpers.
- `src/matrix/`: LED matrix display control.
- `src/sensors/`: hardware sensors.
- `src/system/services/`: ownership and composition root.
- `src/system/init/`: phased initialization.
- `src/system/rtc/`: persistent runtime config.
- `src/system/utils/`: shared infrastructure.
- `src/wifisensing/`: CSI and WiFi sensing.

## Modern ESP32 Patterns

- Use FreeRTOS and ESP-IDF-style patterns for long-lived system code, even though the project builds with `framework = arduino`.
- Use `vTaskDelay(pdMS_TO_TICKS(ms))` instead of `delay()`.
- Use `LOGI`, `LOGW`, and `LOGE` instead of `Serial.print()`.
- Avoid `String` in long-lived services, large buffers, and persistent data.
- Any ISR and functions it calls must be safe for ISR context; use `IRAM_ATTR` where required.
- Treat Arduino as hardware access and ESP-IDF or FreeRTOS as the model for system architecture.
