Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)

# Telegram Integration Reference

## Overview

Telegram is part of the unified notifications subsystem.

Runtime flow:

- runtime settings are stored in `RTC::NotificationData`
- `NotificationSettingsService` exposes those settings over HTTP and persists them
- `TelegramNotifier` queues outbound messages
- `NotificationWorker` drives actual delivery and optional inbound polling
- `TelegramWorker` uses `TelegramClient` for HTTPS calls and command polling
- command handlers live in `src/notifications/telegram/commands/`

The normal operating path is runtime configuration through the notifications API and UI.

## Configuration model

### Runtime API

The main settings endpoint is:

- `GET /api/notifications/settings`
- `POST /api/notifications/settings`

Authentication: admin.

Relevant Telegram fields:

- `telegram_enabled`
- `bot_token`
- `chat_id`
- `commands_enabled`

Important behavior from the current implementation:

- `chat_id` is treated as read-only from the frontend side.
- `chat_id` is auto-discovered from the first incoming Telegram message and then persisted.
- changing `bot_token` clears `chat_id`, forcing discovery again.
- every settings change is applied in RTC immediately and persisted to LittleFS through `CONFIG::save(...)`.

### Factory defaults

Factory fallback values still exist in `src/config/App.h`, but routine operation should
be managed through the runtime notifications settings.

## Runtime behavior

### Outbound notifications

Outbound Telegram delivery goes through:

1. `NotificationSettingsService`
2. `TelegramNotifier`
3. `TELEGRAM::MessageQueue`
4. `NotificationWorker`
5. `TelegramWorker::processOutbound()`
6. `TelegramClient`

Practical consequence:

- having only `bot_token` is enough for Telegram to be considered "enabled/configured" by settings
- actual outbound delivery also needs a valid `chat_id`
- the normal way to obtain `chat_id` is to send any message to the bot once and let the firmware auto-discover it

### Inbound polling and commands

Inbound polling is only active when at least one of these is true:

- `commands_enabled` is true
- `chat_id` still needs auto-discovery

Command dispatch is implemented under `src/notifications/telegram/commands/` and uses
the shared `CommandRegistry`.

## Test endpoint

The test route is:

- `POST /api/notifications/telegram/test`

Authentication: admin.

Request body:

```json
{
  "text": "debug test"
}
```

The backend still accepts legacy `message` for compatibility.

Current behavior is intentionally asynchronous:

- the HTTP handler validates input
- spawns a background task
- returns immediately

Typical success response:

```json
{
  "ok": true,
  "status": "queued"
}
```

Important implication:

- this endpoint confirms that the test task was queued
- it does not wait for Telegram's final HTTP response
- delivery failures must be diagnosed through serial logs and runtime behavior, not from the immediate HTTP response alone

## Network and TLS policy

Telegram delivery is intentionally strict.

Before sending or polling, the firmware requires:

- Wi-Fi mode is not `WIFI_OFF`
- Wi-Fi is connected
- DNS resolution for `api.telegram.org` succeeds
- TCP connect to `api.telegram.org:443` succeeds
- system time is in a valid year window

Those checks are implemented in `src/notifications/telegram/client/TelegramConnectionValidator.cpp`.

### TLS verification

The code supports two TLS modes:

- insecure TLS via `setInsecure()`
- Root CA validation via a pinned PEM certificate

Project nuance:

- `src/config/App.h` defines `TELEGRAM_TLS_VERIFY` default as `0`
- the current `platformio.ini` for this project overrides it to `-DTELEGRAM_TLS_VERIFY=1`

So for the current firmware environment, certificate verification is enabled.

Verification is implemented in `src/notifications/telegram/client/TelegramTlsConfig.cpp`
using a pinned Root CA, not a CA bundle. That keeps firmware size under control on this
target.

## Common failure patterns

The most relevant runtime failure reasons are:

- `offline/wifi_off`
- `offline/wifi_not_connected`
- `offline/dns_failed`
- `offline/tcp_connect_failed`
- `offline/time_invalid`

Common API-layer failures for the test route:

- empty `text` / `message`
- `busy/telegram_test_in_progress`
- task allocation / task creation failure

If Telegram is enabled but nothing is sent, check in this order:

1. Is `telegram_enabled` true?
2. Is `bot_token` set?
3. Has `chat_id` already been discovered or configured?
4. Does the device have internet and valid time?
5. Do serial logs show TLS or Telegram HTTP failures?

## Minimal debug procedure

1. Read current settings from `/api/notifications/settings`.
2. If `chat_id` is empty, send a message to the bot from the target chat.
3. Trigger `/api/notifications/telegram/test`.
4. Inspect serial logs for online-check, TLS, or Telegram API failures.

Use this document as the current operating model. Historical compile-time-only guidance is obsolete for this repo.

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)
