# Clean Workflow

Use the lightest clean that solves the problem. Removing `.pio` destroys the warm
build cache and can turn a 50 second restore into a 15 minute cold build.

## Normal Clean

```bash
./scripts/build-clean.sh
```

or:

```bash
pio run -e waveshare_esp32s3_matrix -t clean
```

## Rebuild Explanation

Before doing a deep clean, check why SCons wants to rebuild:

```bash
./scripts/build-explain.sh
```

If the log only mentions `checkprogsize`, incremental rebuild behavior is
healthy.

## Deep Clean

Use only when package state is corrupted or the task explicitly requires a cold
cache test:

```bash
rm -rf .pio/build
```

For frontend dependency corruption:

```bash
cd interface
rm -rf node_modules build .svelte-kit
npm ci
```

Avoid deleting `.pio/cache` unless you are measuring cold-build behavior.
