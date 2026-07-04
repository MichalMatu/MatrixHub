#!/usr/bin/env bash
set -euo pipefail

pio_bin="${PIO_BIN:-pio}"
if ! command -v "$pio_bin" >/dev/null 2>&1; then
  pio_bin="$HOME/.platformio/penv/bin/pio"
fi

env_name="${PIO_ENV:-waveshare_esp32s3_matrix}"
log_file="${BUILD_EXPLAIN_LOG:-build-explain.log}"

env SCONSFLAGS="--debug=explain" "$pio_bin" run -e "$env_name" "$@" | tee "$log_file"
