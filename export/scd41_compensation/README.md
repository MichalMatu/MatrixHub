# SCD41 Compensation Integration Export

This folder contains the complete SCD41 Compensation module (frontend and backend) exported from the source project.
It is mapped identically to the source project structure to make the transplantation process to a compatible environment straightforward.
This module supports configuring temperature offsets, altitude, and ambient pressure for the SCD41 sensor.

## Structure
- `/frontend`: Contains Svelte/JS UI components, views, and the API service (e.g. `CompensationApiService.ts`).
- `/backend`: Contains the C++ backend code (Service, API, HAL bindings, Configuration objects).
- `/test`: Contains Unity tests for the SCD41 Compensation logic.
- `/docs`: Contains screenshots and documentation describing the compensation logic.

## Instructions for the Next Agent

### 1. Backend Implementation
- Copy the directories under `backend/src/` to your target project's `src/` directory.
- `RtcCompensationTypes.h` handles RTC (Real-Time Clock / deep sleep persistence) typing. Update your master `rtc` state file if your system maintains state across deep sleeps.
- Integrate the `CompensationService` and `CompensationApiService` into your primary runtime orchestrator/Context.
- Make sure to update your main `CMakeLists.txt` or `platformio.ini` to build the new files.

### 2. Frontend Implementation
- Copy the Svelte components in `frontend/lib/` and `frontend/routes/` to the corresponding `interface/src/` paths.
- Ensure the API service `CompensationApiService.ts` is placed in `interface/src/lib/services/api/integrations/`.
- **Translations (Paraglide-js):** Just like the Telegram integration, you need to extract the translation keys corresponding to the `compensation` components. They typically originate from `interface/messages/en.json` and `interface/messages/pl.json`. Check the `.svelte` files to identify precisely which keys are needed (e.g. `m.menu_compensation()`).
- Add the Compensation API instance to your SvelteKit API Context or dependency injection setup.
- Add the route to the main Settings/Sensors navigation menu.

### 3. Verifications
- Run backend unit tests (`pio test -e native` or similar) on the ported test cases under `test/`.
- Build the frontend (`npm run build`) to ensure there are no missing dependencies.
- Deploy to an ESP32 and use a serial monitor to ensure the compensation settings are correctly applied at boot.
