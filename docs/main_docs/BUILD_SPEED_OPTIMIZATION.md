# MatrixHub Build Speed Optimization

Date: 2026-06-19

Target: `waveshare_esp32s3_matrix` on Raspberry Pi 5, 16 GB RAM, Raspberry Pi OS,
project on NVMe, VS Code + PlatformIO/PIOArduino, ESP32-S3 firmware.

The goal was to improve PlatformIO build speed without changing firmware logic.
Only build configuration, helper scripts, VS Code settings, and documentation
were changed.

## Baseline

System:

```text
PlatformIO Core: 6.1.18
Python: 3.13.5
nproc: 4
Kernel: Linux raspberrypi 6.18.33+rpt-rpi-2712 aarch64
```

Measured before optimization:

| Scenario | Result |
| --- | ---: |
| First build with stale UI embed | 6m48.7s |
| No-change build | 39.4s |
| Clean target | 5.3s |
| Clean firmware build | 15m04.3s |
| Real one `.cpp` change plus link | 2m44.5s |

`SCONSFLAGS="--debug=explain"` showed no broad rebuild in the no-change state.
The only selected rebuild reason was:

```text
rebuilding `checkprogsize' because AlwaysBuild() is specified
```

That means PlatformIO/SCons was not rebuilding many object files without source
changes.

## What Was Slow

The slow path was the first full firmware build: many ESP32 framework files,
project files, and library files are compiled, including large dependency trees
such as NimBLE, USB_Driver, Adafruit NeoPixel, and framework Arduino objects.

The UI embed script can also be expensive when UI inputs changed, but it already
had the important protection: deterministic gzip output and write-if-changed for
`lib/framework/core/WWWData.h`. It was not blindly rewriting the generated header
on every build.

PyTorch had no observed impact. It did not appear in PlatformIO output,
dependency graph, scripts used by this build, or the measured build path.

## Changes Made

`platformio.ini`:

```ini
[platformio]
build_cache_dir = .pio/cache
default_envs = waveshare_esp32s3_matrix

[env:waveshare_esp32s3_matrix]
lib_ldf_mode = chain
build_src_filter =
    +<*>
    -<native/**>

[env:waveshare_esp32s3_matrix_dev]
extends = env:waveshare_esp32s3_matrix
custom_skip_ui = yes
```

Why:

- `build_cache_dir` lets PlatformIO reuse compiled objects, ELFs, and firmware
  artifacts between clean build directories when source and build config match.
- `lib_ldf_mode = chain` makes the intended default explicit and avoids drifting
  toward expensive `deep` modes.
- `src/native/**` was compiled as empty ESP32 objects because those files are
  guarded by `NATIVE_BUILD`; excluding them removes pointless firmware work.
- `waveshare_esp32s3_matrix_dev` skips the frontend pipeline for backend-only
  firmware iteration.

`scripts/build/build_interface.py`:

- added support for `custom_skip_ui = yes`,
- kept existing content-stable generation behavior,
- did not change firmware logic.

Helper scripts:

- `scripts/build-fast.sh`: defaults to `waveshare_esp32s3_matrix_dev`, `SKIP_UI=1`,
  and `-j4`.
- `scripts/build-clean.sh`: cleans one env explicitly.
- `scripts/build-explain.sh`: captures SCons explain output.

VS Code:

`.vscode/settings.json` was added locally with:

```json
{
  "platformio-ide.autoRebuildAutocompleteIndex": false,
  "platformio-ide.autoPreloadEnvTasks": false,
  "platformio-ide.activateProjectOnTextEditorChange": false
}
```

Note: `.vscode/` is ignored by this repo, so this setting is useful locally but
is not part of a normal git commit unless force-added intentionally.

## Results After Changes

Representative measurements:

| Scenario | Result |
| --- | ---: |
| First main build after config change and UI script mtime change | 18m21.5s |
| No-change main build | 38.6s |
| First dev env build, UI skipped | 15m38.8s |
| No-change dev env build | 38.7s |
| Clean plus cached restore, `-j2` | 51.3s |
| Clean plus cached restore, `-j3` | 52.4s |
| Clean plus cached restore, `-j4` | 49.7s |

The first build after changing `platformio.ini` is intentionally not treated as
the main improvement metric because changing build configuration invalidates old
signatures. The important result is that a clean build directory can now be
repopulated from `.pio/cache` in about 50 seconds instead of recompiling for
about 15 minutes.

Best measured job value on this Raspberry Pi 5 for the cached restore path:
`-j4`. The difference between `-j2`, `-j3`, and `-j4` was small once cache was hot.
No further full cold-build sweep was run because each cold test costs about
15 minutes and would not change the main conclusion.

Latest `build-explain.log` after changes still shows only:

```text
rebuilding `checkprogsize' because AlwaysBuild() is specified
```

So the incremental rebuild path remains healthy.

## Recommended Workflow

For firmware-only iteration:

```bash
scripts/build-fast.sh
```

For explicit clean:

```bash
scripts/build-clean.sh
```

For diagnosing unexpected rebuilds:

```bash
scripts/build-explain.sh
```

Use the normal release env when UI changes must be embedded or when producing a
final firmware image:

```bash
pio run -e waveshare_esp32s3_matrix
```

## Do Not Touch For Build Speed

- Do not change partition layout, TLS/PSRAM/security defines, USB boot policy, or
  board flags just for faster compilation.
- Do not move to `lib_ldf_mode = deep` or `deep+` unless `chain` cannot resolve a
  real dependency after includes and `lib_deps` are corrected.
- Do not delete the UI embed step from release builds.
- Do not optimize microSD/NVMe for this issue; measurements point to PlatformIO,
  SCons, library scanning, compilation, linking, and generated UI handling.

## Sources Checked

- PlatformIO `build_cache_dir`: https://docs.platformio.org/en/latest/projectconf/sections/platformio/options/directory/build_cache_dir.html
- PlatformIO LDF modes: https://docs.platformio.org/en/latest/librarymanager/ldf.html
- PlatformIO `pio run -j`: https://docs.platformio.org/en/latest/core/userguide/cmd_run.html
- PlatformIO `build_src_filter`: https://docs.platformio.org/en/latest/projectconf/sections/env/options/build/build_src_filter.html
- SCons `--debug=explain`: https://scons.org/doc/production/HTML/scons-man.html
