#!/usr/bin/env python3
"""Decode an ESP32-S3 coredump from the device's flash partition.

Reads the `coredump` partition over USB/UART using `esp-coredump`
(bundled with PlatformIO's penv) and prints a human-readable backtrace
resolved against the firmware ELF.

Typical use (no arguments needed if PlatformIO build is fresh):

    python scripts/diagnostics/decode_coredump.py

Common overrides:

    python scripts/diagnostics/decode_coredump.py --port /dev/cu.usbmodem101
    python scripts/diagnostics/decode_coredump.py --elf build/elf/<hash>.elf
    python scripts/diagnostics/decode_coredump.py --save crashlogs/2026-05-21.bin

The coredump partition itself stays untouched (read-only operation).
"""

from __future__ import annotations

import argparse
import csv
import os
import shutil
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path

# Fallback layout; the script normally reads partitions/partitions_s3.csv.
DEFAULT_COREDUMP_OFFSET = 0x3E7000
DEFAULT_COREDUMP_SIZE = 0x10000
CHIP = "esp32s3"

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_ELF_CANDIDATES = [
    REPO_ROOT / ".pio" / "build" / "waveshare_esp32s3_matrix" / "firmware.elf",
    REPO_ROOT / "build" / "elf" / "latest.elf",
]

PIO_PENV_BIN = Path.home() / ".platformio" / "penv" / "bin"
ESP_COREDUMP_BIN = PIO_PENV_BIN / "esp-coredump"
# esptool v5+ renamed the binary (no .py suffix). Fall back to .py for older venvs.
ESPTOOL_BIN = next(
    (p for p in (PIO_PENV_BIN / "esptool", PIO_PENV_BIN / "esptool.py") if p.exists()),
    PIO_PENV_BIN / "esptool",
)
PARTITION_TABLE = REPO_ROOT / "partitions" / "partitions_s3.csv"
DEFAULT_SERIAL_CANDIDATES = (
    Path(os.environ.get("DEVICE_SERIAL_PORT", "")) if os.environ.get("DEVICE_SERIAL_PORT") else None,
    Path("/dev/ttyACM0"),
    Path("/dev/ttyUSB0"),
)


@dataclass(frozen=True)
class CoredumpPartition:
    offset: int = DEFAULT_COREDUMP_OFFSET
    size: int = DEFAULT_COREDUMP_SIZE


def resolve_elf(user_value: str | None) -> Path:
    if user_value:
        elf = Path(user_value).expanduser().resolve()
        if not elf.is_file():
            sys.exit(f"ELF not found at {elf}")
        return elf
    for candidate in DEFAULT_ELF_CANDIDATES:
        if candidate.is_file():
            return candidate
    sys.exit(
        "Could not find firmware ELF. Build the project first or pass --elf.\n"
        "  Tried: " + ", ".join(str(p) for p in DEFAULT_ELF_CANDIDATES)
    )


def resolve_port(user_value: str | None) -> str | None:
    if not user_value:
        for candidate in DEFAULT_SERIAL_CANDIDATES:
            if candidate and candidate.exists():
                return str(candidate)
        return None

    # esp-coredump does its own auto-detection when --port is omitted.
    # We only validate explicit user input here so the error is clear.
    if not Path(user_value).exists():
        print(
            f"warning: serial port {user_value} does not currently exist; "
            "esp-coredump will retry detection",
            file=sys.stderr,
        )
    return user_value


def resolve_coredump_partition() -> CoredumpPartition:
    if not PARTITION_TABLE.is_file():
        return CoredumpPartition()

    try:
        with PARTITION_TABLE.open(newline="") as fh:
            reader = csv.reader(line for line in fh if not line.lstrip().startswith("#"))
            for row in reader:
                if len(row) < 5:
                    continue
                name = row[0].strip()
                part_type = row[1].strip()
                subtype = row[2].strip()
                if name != "coredump" and subtype != "coredump":
                    continue
                if part_type != "data":
                    continue
                return CoredumpPartition(offset=int(row[3].strip(), 0), size=int(row[4].strip(), 0))
    except Exception as exc:  # noqa: BLE001 - keep diagnostic script resilient.
        print(
            f"warning: could not parse {PARTITION_TABLE}: {exc}; using fallback layout",
            file=sys.stderr,
        )
    return CoredumpPartition()


def ensure_tooling() -> None:
    if not ESP_COREDUMP_BIN.exists():
        sys.exit(
            f"esp-coredump not found at {ESP_COREDUMP_BIN}.\n"
            "Make sure PlatformIO is installed (the penv ships esp-coredump 1.15+)."
        )
    if not ESPTOOL_BIN.exists():
        sys.exit(f"esptool.py not found at {ESPTOOL_BIN}.")


BOOTLOADER_HINT = (
    "esptool could not talk to the chip. This board uses native USB, so it does\n"
    "not always auto-enter bootloader mode from every host state. Try:\n"
    "  python scripts/diagnostics/decode_coredump.py --port /dev/ttyACM0\n"
    "The script will attempt the native-USB 1200-baud touch first. If that still\n"
    "fails, put the ESP32-S3 into download mode manually:\n"
    "  1. hold the BOOT button\n"
    "  2. tap RESET (RST) while still holding BOOT\n"
    "  3. release BOOT, then re-run with --no-touch-bootloader --no-reset\n"
    "After decoding, reset the board once to resume the app."
)


def touch_native_usb_bootloader(port: str | None) -> None:
    if not port:
        print("[0/3] no serial port selected; relying on esptool auto-detection")
        return
    if not Path(port).exists():
        print(f"[0/3] serial port {port} not present; relying on esptool", file=sys.stderr)
        return

    try:
        import serial  # type: ignore[import-not-found]
    except ImportError:
        print(
            "[0/3] pyserial is not installed; skipping 1200-baud bootloader touch",
            file=sys.stderr,
        )
        return

    print(f"[0/3] touching native USB bootloader on {port} at 1200 baud")
    try:
        connection = serial.Serial(port=port, baudrate=1200)
        try:
            connection.dtr = False
        finally:
            connection.close()
    except OSError as exc:
        # Native USB commonly disappears while the host is still closing it.
        # Treat that as informational and give enumeration time to settle.
        print(f"      port re-enumerated during touch: {exc}")
    except Exception as exc:  # noqa: BLE001 - keep going to let esptool report clearly.
        print(f"      warning: bootloader touch failed: {exc}", file=sys.stderr)
    time.sleep(1.5)


def read_partition_to_file(
    port: str | None,
    baud: int,
    dest: Path,
    partition: CoredumpPartition,
    no_reset: bool,
) -> None:
    """Use esptool to dump the raw coredump partition into ``dest``."""
    cmd: list[str] = [str(ESPTOOL_BIN), "--chip", CHIP]
    if port:
        cmd += ["--port", port]
    if no_reset:
        # Caller put the chip into bootloader manually; do not toggle DTR/RTS.
        cmd += ["--before", "no_reset"]
    cmd += [
        "--baud",
        str(baud),
        "read-flash",
        hex(partition.offset),
        hex(partition.size),
        str(dest),
    ]
    print(
        "[1/3] dumping coredump partition "
        f"(offset={hex(partition.offset)} size={hex(partition.size)}) -> {dest}"
    )
    try:
        subprocess.run(cmd, check=True)
    except subprocess.CalledProcessError as exc:
        print("\n" + BOOTLOADER_HINT + "\n", file=sys.stderr)
        raise exc


def partition_is_empty(path: Path) -> bool:
    """Coredump partition fresh from erase is all 0xFF."""
    with path.open("rb") as fh:
        # Sample first 4 KiB; coredump header lives at the start.
        head = fh.read(4096)
    return len(head) == 0 or all(b == 0xFF for b in head)


def decode(core_path: Path, elf: Path, port: str | None, baud: int) -> int:
    cmd: list[str] = [str(ESP_COREDUMP_BIN), "--chip", CHIP]
    if port:
        cmd += ["--port", port]
    cmd += [
        "--baud",
        str(baud),
        "info_corefile",
        "--core",
        str(core_path),
        "--core-format",
        "raw",
        str(elf),
    ]
    print(f"[3/3] decoding via esp-coredump (ELF: {elf.name})")
    return subprocess.run(cmd).returncode


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Decode the ESP32-S3 coredump partition into a readable backtrace.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--port",
        help="Serial port. Default: DEVICE_SERIAL_PORT, /dev/ttyACM0, /dev/ttyUSB0, "
        "then esptool auto-detect.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=460800,
        help="Serial baud rate for read_flash (default: 460800).",
    )
    parser.add_argument(
        "--elf",
        help="Path to firmware ELF with symbols. "
        "Defaults to .pio build, then build/elf/latest.elf.",
    )
    parser.add_argument(
        "--save",
        help="Archive the raw coredump partition image at this path "
        "(also copies the ELF next to it as <save>.elf).",
    )
    parser.add_argument(
        "--no-reset",
        action="store_true",
        help="Skip the DTR/RTS reset pulse before reading. Use this when you "
        "have already put the chip into download mode by holding BOOT and "
        "pressing RESET (required on boards with native USB).",
    )
    parser.add_argument(
        "--touch-bootloader",
        action=argparse.BooleanOptionalAction,
        default=True,
        help="Attempt a native-USB 1200-baud touch before read_flash. "
        "Default: enabled unless --no-touch-bootloader is passed.",
    )
    args = parser.parse_args(argv)

    ensure_tooling()
    elf = resolve_elf(args.elf)
    port = resolve_port(args.port)
    partition = resolve_coredump_partition()

    if args.touch_bootloader and not args.no_reset:
        touch_native_usb_bootloader(port)

    tmpdir = Path(tempfile.mkdtemp(prefix="coredump_"))
    try:
        raw = tmpdir / "coredump.bin"
        read_partition_to_file(port, args.baud, raw, partition, args.no_reset)

        print("[2/3] validating partition contents")
        if partition_is_empty(raw):
            print(
                "Coredump partition appears empty (all 0xFF). "
                "Either the device has not crashed since last erase, "
                "or the coredump was already consumed.",
                file=sys.stderr,
            )
            return 0

        if args.save:
            dest = Path(args.save).expanduser().resolve()
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy(raw, dest)
            shutil.copy(elf, dest.with_suffix(dest.suffix + ".elf"))
            print(f"      archived to {dest} (+ .elf snapshot)")

        return decode(raw, elf, port, args.baud)
    except subprocess.CalledProcessError as exc:
        print(f"command failed: {' '.join(str(a) for a in exc.cmd)}", file=sys.stderr)
        return exc.returncode or 1
    finally:
        shutil.rmtree(tmpdir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
