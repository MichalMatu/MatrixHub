# Build And Upload Workflow

Use this workflow for firmware build decisions.

## Fast Firmware Iteration

Run from the repository root:

```bash
./scripts/build-fast.sh
```

This uses `waveshare_esp32s3_matrix_dev`, skips the UI embed step, and defaults
to `-j4`.

## Full Release Build

Use the main env when UI assets changed or when producing a final firmware image:

```bash
pio run -e waveshare_esp32s3_matrix
```

If `pio` is not in `PATH`, use:

```bash
/home/test/.platformio/penv/bin/pio run -e waveshare_esp32s3_matrix
```

## Upload

Stop any active monitor before upload:

```bash
pkill -f "pio device monitor" || true
pio run -e waveshare_esp32s3_matrix -t upload
```

For backend-only upload during development:

```bash
SKIP_UI=1 pio run -e waveshare_esp32s3_matrix -t upload
```

## Timing Notes

- Cold full build can take about 15 minutes.
- No-change build is about 38-40 seconds.
- Clean build directory restore from warm `.pio/cache` is about 50 seconds.
- Do not run repeated cold clean builds unless the task is specifically about
  PlatformIO/SCons/cache behavior.
