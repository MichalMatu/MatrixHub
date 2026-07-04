Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)

# Telegram Command Reference

Telegram command handling is part of the unified notifications pipeline.

## Current Architecture

Relevant code now lives in:

```text
src/notifications/telegram/
├── client/                         # Shared Telegram HTTP/TLS client
├── runtime/TelegramWorker.*        # Unified inbound/outbound worker logic
├── polling/                        # Long-poll update processing
├── queue/                          # Outbound queue processing
└── commands/
    ├── TelegramCommandTypes.h
    ├── TelegramCommandParser.*
    ├── TelegramCommandDispatcher.*
    ├── CommandRegistry.h
    └── *Command.*                  # Individual command handlers
```

Related runtime wiring:

- `src/system/init/services/NotificationServicesInitializer.cpp`
- `src/notifications/settings/NotificationSettingsService.*`

## How It Works

`TelegramWorker` is the central runtime component.

It handles:

- outbound Telegram notifications queued by the system
- inbound long-polling for Telegram commands
- command dispatch and response enqueueing

The same worker shares the same settings and Telegram client used by the notification pipeline.

## Configuration

Telegram command behavior is configured through the unified notification settings endpoint:

- `GET /api/notifications/settings`
- `POST /api/notifications/settings`

Relevant fields:

- `telegram_enabled`
- `bot_token`
- `chat_id`
- `commands_enabled`

Important behavior:

- `chat_id` is auto-discovered from the first incoming Telegram chat and then persisted
- changing `bot_token` clears the saved `chat_id`
- there is no separate `/rest/telegramCmdSettings` endpoint in the current project

## Command Polling Rules

Inbound polling runs when Telegram is configured and one of these is true:

- `commands_enabled == true`
- `chat_id` has not been discovered yet

That means the device can still auto-discover the first chat even before command execution is fully enabled.

## Available Commands

The current command list is defined in `src/notifications/telegram/commands/CommandRegistry.h`.

At the time of writing, the registry includes:

- `/help`
- `/status`
- `/sensors`
- `/alarms`
- `/triggered`
- `/shelly`
- `/ble`
- `/scripts`
- `/run`
- `/macro_stop`
- `/ip`
- `/matrix`
- `/reboot`
- `/exec`
- `/users`
- `/health`

## Operational Checks

To verify Telegram is configured:

```bash
curl http://<DEVICE_IP>/api/notifications/settings
```

To verify outbound delivery works:

```bash
curl -X POST http://<DEVICE_IP>/api/notifications/telegram/test \
  -H "Content-Type: application/json" \
  -d '{"text":"telegram test"}'
```

There is currently no dedicated REST endpoint only for Telegram command worker status. Runtime visibility is exposed through system telemetry/UI integration and broadcaster updates.

## Adding a New Command

1. Add a new handler under `src/notifications/telegram/commands/`.
2. Use `TelegramCommandTypes.h` for the command context contract.
3. Register the handler in `src/notifications/telegram/commands/CommandRegistry.h`.
4. Keep responses bounded and avoid heap-heavy formatting inside handlers.

Minimal shape:

```cpp
namespace TELEGRAM::Commands {

bool handleMyCommand(CommandContext& ctx);

}
```

Then add it to `kCommands[]` in `CommandRegistry.h`.

## Troubleshooting

If commands do not respond:

1. Check `telegram_enabled`, `bot_token`, and `commands_enabled` in `/api/notifications/settings`.
2. If `chat_id` is empty, send a Telegram message to the bot once to allow auto-discovery.
3. Verify outbound Telegram delivery with `/api/notifications/telegram/test`.
4. Check WiFi connectivity and logs.

If commands are ignored from the wrong chat:

- authorization is tied to the stored `chat_id`
- the worker accepts commands only from the authorized chat once `chat_id` is known

If you add commands:

- update `CommandRegistry.h`
- keep command names stable because `/help` and dispatch both rely on that registry

Navigation: [Project README](../../../README.md) · [Engineering Reference](../README.md) · [Integrations](../README.md#integrations-and-specialized-subsystems)
