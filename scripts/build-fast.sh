#!/usr/bin/env bash
set -euo pipefail

pio_bin="${PIO_BIN:-pio}"
if ! command -v "$pio_bin" >/dev/null 2>&1; then
  pio_bin="$HOME/.platformio/penv/bin/pio"
fi

env_name="${PIO_ENV:-waveshare_esp32s3_matrix_dev}"
jobs="${PIO_JOBS:-4}"

exec env SKIP_UI=1 "$pio_bin" run -e "$env_name" -j "$jobs" "$@"
