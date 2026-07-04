# MatrixHub User Guide

This is the maintained operator-facing guide. It intentionally avoids static
screenshots and long walkthroughs; the UI changes faster than images.

## Access

- Default device URL: `https://192.168.0.30`
- mDNS alias when available: `https://matrixhub.local`
- Browser-friendly entry is also supported: typing `192.168.0.30` or
  `matrixhub.local` redirects from HTTP to HTTPS automatically.
- Authentication uses the firmware JWT login flow.
- Admin-only pages can be visible but disabled for non-admin users.

## Main Routes

| Route | Purpose |
| --- | --- |
| `/` | Dashboard overview, live status, alarms, and feature summaries. |
| `/wifi/sta` | Connect the device to a home Wi-Fi network. |
| `/wifi/ap` | Configure the device access point. |
| `/alarms` | Create and manage threshold alarms and notification actions. |
| `/charts` | Compare retained CO2, temperature, and humidity history on a shared timeline. |
| `/logs` | Browse and download stored sensor logs. |
| `/bluetooth` | Manage scanner-only BLE sensors and discovered devices. |
| `/shelly` | Add and control Shelly relay devices. |
| `/wifisensing` | Configure RSSI-based Wi-Fi sensing. |
| `/wifisensing/csi` | Inspect CSI diagnostics and live CSI stream state. |
| `/connections/ntp` | Configure time synchronization. |
| `/settings/notifications/telegram` | Configure Telegram alerts and command polling. |
| `/settings/notifications/pushover` | Configure Pushover alerts. |
| `/settings/notifications/webhook` | Configure Webhook alerts. |
| `/settings/notifications/heartbeat` | Configure heartbeat monitoring. |
| `/settings/integrations/udp` | Configure UDP data push. |
| `/system/status` | Inspect runtime, task, heap, network, and diagnostic status. |
| `/system/power` | Configure sleep and power behavior. |
| `/system/matrix` | Configure matrix LED display behavior. |
| `/system/styles` | Configure UI theme/style options. |
| `/system/file-manager` | Browse and manage files when enabled. |
| `/system/help` | Built-in support and troubleshooting page. |
| `/user` | Manage users and account settings. |
| `/usb-features/airmouse` | Configure Air Mouse. |
| `/usb-features/jiggler` | Configure Mouse Jiggler. |
| `/usb-features/keyboard` | Use direct keyboard features. |
| `/usb-features/macros` | Manage macro scripts. |
| `/usb-features/terminal` | Use the USB terminal feature. |

## Normal Setup Flow

1. Open the device over HTTPS.
2. Sign in.
3. Configure `/wifi/sta` if the device is not on the home network yet.
4. Confirm time sync under `/connections/ntp`.
5. Configure notifications under `/settings/notifications/*`.
6. Add sensors, Shelly devices, alarms, and optional USB/Wi-Fi sensing features.
7. Check `/system/status` after major configuration changes.

## Troubleshooting

- If the UI is unreachable, confirm whether the device is in STA mode or AP mode.
- If Telegram or Webhook notifications fail, check Wi-Fi connectivity and valid
  time first.
- If CSI is unavailable, confirm STA connectivity and check `/wifisensing/csi`.
- If a settings save says a restart is required, reboot before treating runtime
  behavior as final.

For implementation details, use the engineering docs in `docs/engineering/`.
