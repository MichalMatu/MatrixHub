# File Manager Service

Standardised helpers for the dashboard file manager.

- `api.ts` – orchestrated actions for listing, uploads, downloads, and system state probing.
- `utils.ts` – pure utilities (state derivation, label helpers).
- `types.ts` – public types consumed by stores and components.

API guardrails:

- `list` uses the `dir` query parameter; `download`, `remove`, and `upload` use `path`.
- Paths are canonicalized before requests are sent. Parent traversal, backslashes, control
  characters, and unsupported `/sdcard` access are rejected client-side and still enforced
  by firmware.
- Uploads to protected config and log storage (`/config`, `/data`, plus their
  `/littlefs/...` aliases) are blocked before the browser opens a transfer. Historical log
  removal remains client-valid; active-log protection is enforced by firmware.

Consumers should import from `$lib/services/fileManager` to access the barrel exports.
