# Telegram Integration Export

This folder contains the complete Telegram integration module (frontend and backend) exported from the source project.
It is mapped identically to the source project structure to make the transplantation process to a compatible environment straightforward.

## Structure
- `/frontend`: Contains Svelte/JS UI components and routes.
- `/backend`: Contains the C++ backend code (Client, Commands, Notifications, Configuration).
- `/test`: Contains Unity tests for the Telegram logic.
- `/docs`: Contains engineering and user documentation.

## Instructions for the Next Agent

### 1. Backend Implementation
- Move the contents of `backend/src/notifications/telegram` to the target project's `src/` directory (maintaining the path structure).
- Move the broadcaster (`TelegramStatusBroadcaster`) and sender APIs (`TelegramTestSender`) to the target project's API layers.
- If the target project relies on dependency injection or runtime contexts, instantiate `TelegramWorker` and `TelegramPoller` and add them to the main configuration view / task manager.
- Verify `platformio.ini` includes necessary SSL and JSON dependencies (e.g. `ArduinoJson`).

### 2. Frontend Implementation
- Copy the Svelte components in `frontend/lib/` and the routes in `frontend/routes/` to the corresponding `interface/src/` path.
- **Translations (Paraglide-js):** Ensure that the translation keys prefixed with `telegram_` or used in these Svelte files are ported over to the new project's translation dictionary (e.g., `messages/en.json` / `messages/pl.json`).
- If there is a global Settings/Notifications menu in the target frontend project, add a link to the extracted Telegram route.

### 3. Verifications
- Run backend unit tests (`pio test -e native` or similar) on the ported test cases under `test/`.
- Build the frontend (`npm run build`) to ensure there are no missing types or unused imports.
