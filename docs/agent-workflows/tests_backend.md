# Backend Test Workflow

Run host-side Unity tests through the PlatformIO native env:

```bash
pio test -e native
```

If `pio` is not in `PATH`:

```bash
/home/test/.platformio/penv/bin/pio test -e native
```

Focused suites:

```bash
pio test -e native -f "test_ble_device_type_detector"
pio test -e native -f "test_telegram_json_parsers"
```

Focused power/deep-sleep suites:

```bash
pio test -e native -f "test_power_sleep" -f "test_power_activity_logging" -f "test_power_api_service" -f "test_config_system" -f "test_rtc_section_defaults"
```

Coverage env:

```bash
pio test -e native_coverage
```

Add or update native tests for firmware logic that can run without ESP32
hardware. For ESP32-only behavior, document the build/log/static verification
used.

For a flashed device, the optional deep-sleep smoke path is:

```bash
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --power-sleep-smoke --power-sleep-duration-ms 30000
```

This test temporarily sleeps the device, waits for timer wake, and restores the
power configuration.

When adding or reviewing power config tests, include the safety invariant:
`sleep_enabled=true` requires at least one wake source. Invalid direct API
payloads should return `400 input/power_wake_source_required`.
