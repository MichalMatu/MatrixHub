# Core Dump Workflow

Use this workflow to diagnose a panic or task watchdog reset.

## Preferred Decoder

Run from the repository root:

```bash
python scripts/diagnostics/decode_coredump.py
```

Common options:

```bash
python scripts/diagnostics/decode_coredump.py --port /dev/ttyACM0
python scripts/diagnostics/decode_coredump.py --elf .pio/build/waveshare_esp32s3_matrix/firmware.elf
python scripts/diagnostics/decode_coredump.py --save crashlogs/latest-coredump.bin
```

## Partition Details

The current partition table defines:

- coredump offset: `0x3E7000`
- coredump size: `0x010000`

Source of truth: `partitions/partitions_s3.csv`.

## Manual Fallback

```bash
python3 -m esptool --port /dev/ttyACM0 --baud 921600 read_flash 0x3E7000 0x10000 coredump_latest.bin
dd if=coredump_latest.bin of=coredump.elf bs=1 skip=24
/home/test/.platformio/packages/tool-xtensa-esp-elf-gdb/bin/xtensa-esp32s3-elf-gdb \
  --batch \
  -ex "info threads" \
  -ex "thread apply all bt" \
  .pio/build/waveshare_esp32s3_matrix/firmware.elf \
  coredump.elf > coredump_report.txt
```

Check `coredump_report.txt` for blocked tasks, watchdog victims, and long I/O
paths.
