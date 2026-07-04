# USB Keyboard Integration Export

This folder contains the complete USB Keyboard module (frontend, backend, and driver) exported from the source project.
It allows acting as a USB HID Device (specifically a Keyboard) and sending keystrokes from the web UI to the host device.

## Structure
- `/frontend`: Contains Svelte/JS UI components, views, keyboard layouts, and the API service (e.g. `KeyboardApiService.ts`).
- `/backend`: Contains the ESP32 backend logic (`KeyboardService`, APIs, HAL, and RTC types).
- `/backend/lib/USB_Driver`: Contains the underlying C++ libraries enabling USB HID Keyboard emulation and mapping multiple regional layouts.
- `/test`: Contains Unity tests for the keyboard backend logic and required stubs.
- `/docs`: Contains Markdown documentation and screenshots representing the user guide.

## Instructions for the Next Agent

### 1. Driver & Dependencies (ESP32-S3 / USB OTG required)
- Ensure the destination board supports native USB (e.g. ESP32-S2 or ESP32-S3).
- Copy the contents of `backend/lib/USB_Driver` into the target's `lib/` directory or appropriately vendored dependencies folder. Ensure these are built alongside the project.

### 2. Backend Implementation
- Copy the structures from `backend/src/` to your target project's `src/` directory.
- `RtcKeyboardTypes.h` handles deep sleep state retention for keyboard layout settings. Incorporate it into your main RTC state manager.
- Instantiate `KeyboardService` within your primary App runtime/Context and register the `KeyboardApiService` with the network server.
- Bind the underlying TinyUSB / USB HID stack to the ESP32's native USB peripheral in your main `setup()` routine (if not already handled globally).

### 3. Frontend Implementation
- Move the components and route (`frontend/lib/`, `frontend/routes/`) into the target SvelteKit application (`interface/src/`).
- The `KeyboardApiService.ts` must be included in your `interface/src/lib/services/api/integrations/` directory and injected wherever the API context is established.
- **Translations:** Ensure translation keys prefixed with `keyboard_` from `messages/en.json` / `messages/pl.json` are present in the new frontend's dictionary. Take special note of dynamically injected layout names or error messages inside the Svelte files.

### 4. Verifications
- Run backend unit tests (`pio test -e native` or similar) against the ported test files in `test/`. The suite includes stubs for the HID hardware which allows testing the logic without a physical USB connection.
- Flash the resulting firmware to an ESP32-S3, plug the native USB port into a computer, and verify if it enumerates properly as a standard "USB Input Device". Open the web interface to dispatch keystrokes.
