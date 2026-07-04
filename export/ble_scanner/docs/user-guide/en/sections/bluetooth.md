# Bluetooth Devices

Navigation: [Home](../README.md) · [Basic Flows](../README.md#basic-use-cases) · [Additional Flows](../README.md#additional-use-cases) · [Reference](../README.md#reference-sections)

Use the `Bluetooth` page to enable the BLE scanner, scan for supported sensors,
and keep a saved list of wireless probes that MatrixHub should watch.

Users with management access can scan and edit the saved device list. Read-only
users can still use the page as a status view.

## Empty State

If no devices are saved yet, the page starts with an empty state:

![Bluetooth empty state](../screenshots/bluetooth/bluetooth-empty-state.png)

This is the fastest place to start the first scan.

## Scanner Settings and Status

The page combines a runtime status card with a BLE scanner settings card.

## Important Behavior

- `Add Device` only works when the BLE scanner is enabled and the setting is
  saved
- if scanning is disabled in settings, the saved device area becomes read-only
- the status card helps you distinguish between BLE being available and the
  scanner actively running at that moment

## Scan Devices

Use `Add Device` to start a scan and review detected sensors:

![Bluetooth scan modal](../screenshots/bluetooth/bluetooth-scan-modal.png)

The scan modal shows:

- MAC address
- live temperature and humidity
- battery level
- RSSI signal strength

Saved devices are marked as `Added`, so you can avoid adding the same probe
twice.

## Device List

After adding devices, the page shows live readings, battery level, signal
strength, and scanner status:

![Bluetooth device list](../screenshots/bluetooth/bluetooth-device-list.png)

Each saved row can show:

- alias and MAC address
- temperature and humidity
- battery level
- RSSI
- time since the last packet was received

If the sensor has not reported yet, the row can stay in a waiting state until
fresh BLE data arrives.

## Rename Device

You can also rename a saved device to make the list easier to read:

![Bluetooth edit alias modal](../screenshots/bluetooth/bluetooth-edit-alias-modal.png)

Renaming only changes the local alias. It does not change the saved MAC
address.

Bluetooth is useful when you want to:

- add external BLE temperature or humidity sensors
- compare internal readings with nearby wireless probes
- keep sensor names readable for daily monitoring
- watch several rooms or shelves from the same dashboard

Navigation: [Home](../README.md) · [Basic Flows](../README.md#basic-use-cases) · [Additional Flows](../README.md#additional-use-cases) · [Reference](../README.md#reference-sections)
