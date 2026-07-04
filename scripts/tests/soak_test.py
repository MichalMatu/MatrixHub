#!/usr/bin/env python3
"""Long-running RAM / task watermark observer for the MatrixHub firmware.

Polls `/api/system/info` and `/api/system/tasks` at a fixed interval and
appends a CSV row per sample. Designed to be left running for hours or days
to catch slow memory leaks, fragmentation drift, and stack-watermark erosion
that don't show up in `stress_test.py`'s burst-style hammering.

Typical run before a release:

    python scripts/tests/soak_test.py --duration 24h --plot

Quick local sanity check:

    python scripts/tests/soak_test.py --duration 5m --interval 10s --plot

CSV columns:
    timestamp_iso, uptime_sec, free_heap, min_free_heap, used_heap,
    free_psram, used_psram, core_temp_c, task_count,
    min_stack_watermark, min_stack_task_name, runtime counters,
    sample_latency_ms, sample_errors
"""

from __future__ import annotations

import argparse
import csv
import datetime
import json
import re
import signal
import statistics
import sys
import time
from pathlib import Path
from typing import Any, Iterable
from urllib.parse import urlsplit

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from device_client import DeviceClient, DeviceClientError, add_common_device_args  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_REPORT_DIR = REPO_ROOT / "artifacts" / "soak"

CSV_FIELDS = [
    "timestamp_iso",
    "uptime_sec",
    "free_heap",
    "min_free_heap",
    "used_heap",
    "free_psram",
    "used_psram",
    "core_temp_c",
    "task_count",
    "min_stack_watermark",
    "min_stack_task_name",
    "stack_details_available",
    "internal_largest_block",
    "internal_fragmentation_percent",
    "psram_largest_block",
    "psram_fragmentation_percent",
    "ws_queue_drops",
    "ws_heap_fallbacks",
    "lock_standard_timeouts",
    "lock_recursive_timeouts",
    "lock_standard_slow_acquires",
    "lock_recursive_slow_acquires",
    "boot_count",
    "unexpected_restarts",
    "current_reset_reason",
    "sample_latency_ms",
    "sample_errors",
]

DURATION_RE = re.compile(r"^\s*(\d+)\s*([smhd]?)\s*$", re.IGNORECASE)


def parse_duration(text: str) -> float:
    """Accept "30s", "5m", "24h", "1d" or a raw integer (seconds). "0" = infinite."""
    if not text:
        raise argparse.ArgumentTypeError("empty duration")
    match = DURATION_RE.match(text)
    if not match:
        raise argparse.ArgumentTypeError(f"unrecognized duration: {text!r}")
    value = int(match.group(1))
    unit = match.group(2).lower() or "s"
    multiplier = {"s": 1, "m": 60, "h": 3600, "d": 86400}[unit]
    return float(value * multiplier)


def parse_interval(text: str) -> float:
    seconds = parse_duration(text)
    if seconds < 1:
        raise argparse.ArgumentTypeError("interval must be >= 1 second")
    return seconds


def safe_slug(value: str) -> str:
    return "".join(ch if ch.isalnum() or ch in {"-", "_"} else "-" for ch in value).strip("-")


def login_with_retry(client: DeviceClient, max_wait_s: float = 75.0) -> str:
    if client.token:
        return client.login()

    deadline = time.monotonic() + max(0.0, max_wait_s)
    while True:
        response = client.session.post(
            f"{client.base_url}/rest/signIn",
            json={"username": client.username, "password": client.password},
            timeout=client.timeout,
        )
        if response.status_code == 429 and time.monotonic() < deadline:
            print("[AUTH] login rate limited; waiting 5s", file=sys.stderr, flush=True)
            time.sleep(min(5.0, max(0.1, deadline - time.monotonic())))
            continue
        if response.status_code != 200:
            raise DeviceClientError(
                f"signIn failed: HTTP {response.status_code} {response.text[:200]}"
            )
        try:
            data = response.json()
        except ValueError as exc:
            raise DeviceClientError("signIn returned non-JSON response") from exc
        token = data.get("access_token")
        if not isinstance(token, str) or not token.strip():
            raise DeviceClientError("signIn succeeded but no access_token was returned")
        client.set_token(token)
        return token


def get_with_auth(
    client: DeviceClient,
    path: str,
) -> dict | None:
    try:
        response = client.get(path)
    except Exception as exc:
        print(f"  warning: GET {path} failed: {exc}", file=sys.stderr)
        return None

    if not response.ok:
        print(
            f"  warning: GET {path} returned {response.status_code}",
            file=sys.stderr,
        )
        return None
    try:
        return response.json()
    except ValueError:
        print(f"  warning: GET {path} returned non-JSON body", file=sys.stderr)
        return None


def nested(data: dict[str, Any], path: tuple[str, ...], default: Any = None) -> Any:
    current: Any = data
    for key in path:
        if not isinstance(current, dict):
            return default
        current = current.get(key)
    return current if current is not None else default


def extract_sample(
    info: dict | None,
    tasks: dict | None,
    summary: dict | None,
    mutexes: dict | None,
) -> dict:
    info = info or {}
    tasks = tasks or {}
    summary = summary or {}
    mutexes = mutexes or {}
    task_list = tasks.get("tasks") or []
    task_stack = tasks.get("stack") or {}
    task_memory = tasks.get("memory") or {}
    internal = task_memory.get("internal") or {}
    psram = task_memory.get("psram") or {}

    min_watermark = None
    min_watermark_name = ""
    for task in task_list:
        wm = task.get("stackHighWaterMark")
        if wm is None:
            continue
        if min_watermark is None or wm < min_watermark:
            min_watermark = wm
            min_watermark_name = task.get("name", "")

    if min_watermark is None and isinstance(task_stack, dict):
        min_watermark = task_stack.get("worstHighWaterMark")
        min_watermark_name = task_stack.get("worstTask", "")

    return {
        "timestamp_iso": datetime.datetime.now(datetime.timezone.utc)
        .isoformat(timespec="seconds"),
        "uptime_sec": info.get("uptime"),
        "free_heap": info.get("free_heap"),
        "min_free_heap": info.get("min_free_heap"),
        "used_heap": info.get("used_heap"),
        "free_psram": info.get("free_psram"),
        "used_psram": info.get("used_psram"),
        "core_temp_c": info.get("core_temp"),
        "task_count": tasks.get("taskCount"),
        "min_stack_watermark": min_watermark,
        "min_stack_task_name": min_watermark_name,
        "stack_details_available": bool(task_stack.get("detailsAvailable")),
        "internal_largest_block": internal.get("largestBlock") or info.get("max_alloc_heap"),
        "internal_fragmentation_percent": internal.get("fragmentationPercent"),
        "psram_largest_block": psram.get("largestBlock"),
        "psram_fragmentation_percent": psram.get("fragmentationPercent"),
        "ws_queue_drops": nested(summary, ("http", "wsQueueDrops")),
        "ws_heap_fallbacks": nested(summary, ("http", "wsHeapFallbacks")),
        "lock_standard_timeouts": nested(mutexes, ("runtime", "standard", "timeouts")),
        "lock_recursive_timeouts": nested(mutexes, ("runtime", "recursive", "timeouts")),
        "lock_standard_slow_acquires": nested(mutexes, ("runtime", "standard", "slowAcquires")),
        "lock_recursive_slow_acquires": nested(mutexes, ("runtime", "recursive", "slowAcquires")),
        "boot_count": nested(summary, ("boot", "bootCount")),
        "unexpected_restarts": nested(summary, ("boot", "unexpectedRestarts")),
        "current_reset_reason": nested(summary, ("boot", "currentResetReason")),
    }


def summarize(samples: list[dict], threshold: float) -> int:
    if not samples:
        print("no samples collected")
        return 1

    def column(key: str) -> list[int]:
        return [s[key] for s in samples if s.get(key) is not None]

    free = column("free_heap")
    min_free = column("min_free_heap")
    psram = column("free_psram")
    watermark = column("min_stack_watermark")
    internal_largest = column("internal_largest_block")
    psram_largest = column("psram_largest_block")
    ws_drops = column("ws_queue_drops")
    ws_fallbacks = column("ws_heap_fallbacks")
    std_timeouts = column("lock_standard_timeouts")
    rec_timeouts = column("lock_recursive_timeouts")
    std_slow = column("lock_standard_slow_acquires")
    rec_slow = column("lock_recursive_slow_acquires")
    boot_counts = column("boot_count")
    restarts = column("unexpected_restarts")
    sample_latencies = [float(s["sample_latency_ms"]) for s in samples if s.get("sample_latency_ms") is not None]
    sample_errors = column("sample_errors")

    print("\n=== soak summary ===")
    print(f"samples              : {len(samples)}")
    print(f"request count        : {len(samples) * 4}")
    if sample_errors:
        print(f"sample errors total  : {sum(sample_errors)}")
    if sample_latencies:
        p95_index = min(
            len(sample_latencies) - 1,
            max(0, int(round((len(sample_latencies) - 1) * 0.95))),
        )
        print(
            f"sample latency avg/p95/max: "
            f"{statistics.mean(sample_latencies):.2f} / "
            f"{sorted(sample_latencies)[p95_index]:.2f} / "
            f"{max(sample_latencies):.2f} ms"
        )
    if free:
        print(
            f"free_heap     min/med/max: {min(free)} / "
            f"{int(statistics.median(free))} / {max(free)}"
        )
    if min_free:
        first, last = min_free[0], min_free[-1]
        drift = first - last
        drift_pct = drift / first if first else 0.0
        print(
            f"min_free_heap first/last  : {first} -> {last} "
            f"(drift {drift:+d} = {drift_pct:+.1%})"
        )
    if psram:
        print(
            f"free_psram    min/med/max: {min(psram)} / "
            f"{int(statistics.median(psram))} / {max(psram)}"
        )
    if internal_largest:
        print(
            f"internal largest min/med/max: {min(internal_largest)} / "
            f"{int(statistics.median(internal_largest))} / {max(internal_largest)}"
        )
    if psram_largest:
        print(
            f"psram largest    min/med/max: {min(psram_largest)} / "
            f"{int(statistics.median(psram_largest))} / {max(psram_largest)}"
        )
    if watermark:
        worst = min(watermark)
        worst_idx = watermark.index(worst)
        worst_task = [
            s["min_stack_task_name"]
            for s in samples
            if s.get("min_stack_watermark") is not None
        ][worst_idx]
        print(f"worst stack watermark    : {worst} bytes (task: {worst_task})")
    if ws_drops:
        print(f"ws_queue_drops first/last: {ws_drops[0]} -> {ws_drops[-1]}")
    if ws_fallbacks:
        print(f"ws_heap_fallbacks first/last: {ws_fallbacks[0]} -> {ws_fallbacks[-1]}")
    if std_timeouts or rec_timeouts:
        print(
            "lock timeouts first/last : "
            f"standard {std_timeouts[0] if std_timeouts else 0} -> {std_timeouts[-1] if std_timeouts else 0}, "
            f"recursive {rec_timeouts[0] if rec_timeouts else 0} -> {rec_timeouts[-1] if rec_timeouts else 0}"
        )
    if std_slow or rec_slow:
        print(
            "slow lock acquires       : "
            f"standard {std_slow[0] if std_slow else 0} -> {std_slow[-1] if std_slow else 0}, "
            f"recursive {rec_slow[0] if rec_slow else 0} -> {rec_slow[-1] if rec_slow else 0}"
        )

    if min_free and len(min_free) >= 2:
        first, last = min_free[0], min_free[-1]
        if first > 0 and (first - last) / first > threshold:
            print(
                f"\nFAIL: min_free_heap dropped more than "
                f"{threshold:.0%} (configured --regression-threshold)"
            )
            return 1
    if boot_counts and len(boot_counts) >= 2 and boot_counts[-1] != boot_counts[0]:
        print(f"\nFAIL: boot_count changed during soak ({boot_counts[0]} -> {boot_counts[-1]})")
        return 1
    if restarts and len(restarts) >= 2 and restarts[-1] != restarts[0]:
        print(
            "\nFAIL: unexpected_restarts changed during soak "
            f"({restarts[0]} -> {restarts[-1]})"
        )
        return 1
    if ws_drops and len(ws_drops) >= 2 and ws_drops[-1] != ws_drops[0]:
        print(f"\nFAIL: ws_queue_drops changed during soak ({ws_drops[0]} -> {ws_drops[-1]})")
        return 1
    if std_timeouts and len(std_timeouts) >= 2 and std_timeouts[-1] != std_timeouts[0]:
        print(
            "\nFAIL: standard lock timeouts changed during soak "
            f"({std_timeouts[0]} -> {std_timeouts[-1]})"
        )
        return 1
    if rec_timeouts and len(rec_timeouts) >= 2 and rec_timeouts[-1] != rec_timeouts[0]:
        print(
            "\nFAIL: recursive lock timeouts changed during soak "
            f"({rec_timeouts[0]} -> {rec_timeouts[-1]})"
        )
        return 1
    if sample_errors and sum(sample_errors) > 0:
        print(f"\nFAIL: {sum(sample_errors)} sample request(s) failed during soak")
        return 1
    print("\nPASS")
    return 0


def numeric_column(samples: list[dict], key: str) -> list[int]:
    return [s[key] for s in samples if isinstance(s.get(key), int)]


def float_column(samples: list[dict], key: str) -> list[float]:
    return [float(s[key]) for s in samples if s.get(key) is not None]


def delta_for(samples: list[dict], key: str) -> int | None:
    values = numeric_column(samples, key)
    if len(values) < 2:
        return None
    return values[-1] - values[0]


def percentile(values: list[float], pct: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, max(0, int(round((len(ordered) - 1) * pct))))
    return round(ordered[index], 2)


def build_report(
    *,
    target: str,
    started_at: str,
    finished_at: str,
    output_path: Path,
    samples: list[dict],
    exit_code: int,
) -> dict[str, Any]:
    latencies = float_column(samples, "sample_latency_ms")
    sample_errors = numeric_column(samples, "sample_errors")
    watermarks = numeric_column(samples, "min_stack_watermark")
    worst_stack_sample = min(
        (s for s in samples if isinstance(s.get("min_stack_watermark"), int)),
        key=lambda s: s["min_stack_watermark"],
        default={},
    )
    return {
        "ok": exit_code == 0,
        "target": target,
        "started_at": started_at,
        "finished_at": finished_at,
        "csv": str(output_path),
        "summary": {
            "samples": len(samples),
            "request_count": len(samples) * 4,
            "failed_sample_requests": sum(sample_errors),
            "avg_latency_ms": round(statistics.mean(latencies), 2) if latencies else 0.0,
            "p95_latency_ms": percentile(latencies, 0.95),
            "max_latency_ms": round(max(latencies), 2) if latencies else 0.0,
        },
        "runtime": {
            "heap_drift": {
                "free_heap": delta_for(samples, "free_heap"),
                "min_free_heap": delta_for(samples, "min_free_heap"),
                "free_psram": delta_for(samples, "free_psram"),
                "internal_largest_block": delta_for(samples, "internal_largest_block"),
                "psram_largest_block": delta_for(samples, "psram_largest_block"),
            },
            "min_stack_watermark": min(watermarks) if watermarks else None,
            "min_stack_task": worst_stack_sample.get("min_stack_task_name"),
            "lock_contention_delta": {
                "standard_timeouts": delta_for(samples, "lock_standard_timeouts"),
                "recursive_timeouts": delta_for(samples, "lock_recursive_timeouts"),
                "standard_slow_acquires": delta_for(samples, "lock_standard_slow_acquires"),
                "recursive_slow_acquires": delta_for(samples, "lock_recursive_slow_acquires"),
            },
            "ws_queue_drops_delta": delta_for(samples, "ws_queue_drops"),
            "reset_detection": {
                "boot_count_delta": delta_for(samples, "boot_count"),
                "unexpected_restarts_delta": delta_for(samples, "unexpected_restarts"),
            },
        },
        "first_sample": samples[0] if samples else None,
        "last_sample": samples[-1] if samples else None,
    }


def render_report_markdown(report: dict[str, Any]) -> str:
    summary = report["summary"]
    runtime = report["runtime"]
    lines = [
        "# MatrixHub Soak Report",
        "",
        f"- Target: `{report['target']}`",
        f"- Started: `{report['started_at']}`",
        f"- Finished: `{report['finished_at']}`",
        f"- Result: `{'PASS' if report['ok'] else 'FAIL'}`",
        f"- CSV: `{report['csv']}`",
        f"- Samples: `{summary['samples']}`",
        f"- Request count: `{summary['request_count']}`",
        f"- Failed sample requests: `{summary['failed_sample_requests']}`",
        f"- Average latency: `{summary['avg_latency_ms']} ms`",
        f"- P95 latency: `{summary['p95_latency_ms']} ms`",
        f"- Max latency: `{summary['max_latency_ms']} ms`",
        "",
        "## Runtime",
        "",
        f"- Heap drift: `{runtime['heap_drift']}`",
        f"- Min stack watermark: `{runtime['min_stack_watermark']}` (`{runtime['min_stack_task']}`)",
        f"- Lock contention delta: `{runtime['lock_contention_delta']}`",
        f"- WS queue drops delta: `{runtime['ws_queue_drops_delta']}`",
        f"- Reset detection: `{runtime['reset_detection']}`",
    ]
    return "\n".join(lines) + "\n"


def write_report(report: dict[str, Any], report_dir: Path) -> dict[str, str]:
    report_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    target_slug = safe_slug(urlsplit(report["target"]).netloc or "device")
    base = f"soak-{target_slug}-{stamp}"
    json_path = report_dir / f"{base}.json"
    md_path = report_dir / f"{base}.md"
    paths = {"json": str(json_path), "markdown": str(md_path)}
    with_paths = dict(report)
    with_paths["reports"] = paths
    json_path.write_text(json.dumps(with_paths, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    md_path.write_text(render_report_markdown(with_paths), encoding="utf-8")
    return paths


def maybe_plot(samples: list[dict], output: Path) -> None:
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt  # noqa: WPS433
    except ImportError:
        print("matplotlib not installed; skipping --plot", file=sys.stderr)
        return

    times = list(range(len(samples)))
    fig, axes = plt.subplots(4, 1, figsize=(10, 12), sharex=True)
    series: Iterable[tuple] = (
        ("free_heap", "free heap (bytes)"),
        ("min_free_heap", "min free heap (bytes)"),
        ("free_psram", "free psram (bytes)"),
        ("min_stack_watermark", "worst stack watermark (bytes)"),
    )
    for ax, (key, label) in zip(axes, series):
        values = [s.get(key) for s in samples]
        ax.plot(times, values, marker=".", linewidth=1)
        ax.set_ylabel(label)
        ax.grid(True, alpha=0.3)
    axes[-1].set_xlabel("sample index")
    fig.suptitle(f"soak test: {output.name}")
    plot_path = output.with_suffix(".png")
    fig.tight_layout()
    fig.savefig(plot_path, dpi=120)
    print(f"plot saved to {plot_path}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    add_common_device_args(parser)
    parser.add_argument(
        "--interval",
        type=parse_interval,
        default=30.0,
        help="Seconds between samples (e.g. 30, 30s, 1m). Default 30s.",
    )
    parser.add_argument(
        "--duration",
        type=parse_duration,
        default=parse_duration("24h"),
        help="Total run time (e.g. 24h, 30m, 0=infinite). Default 24h.",
    )
    parser.add_argument(
        "--output",
        default=None,
        help="CSV output path. Default: artifacts/soak/soak_<UTC_timestamp>.csv",
    )
    parser.add_argument(
        "--regression-threshold",
        type=float,
        default=0.10,
        help="Fail if (first_min_free - last_min_free) / first_min_free "
        "exceeds this fraction. Default 0.10 (10%%).",
    )
    parser.add_argument(
        "--plot",
        action="store_true",
        help="Generate <output>.png chart at the end (requires matplotlib).",
    )
    parser.add_argument("--login-timeout-sec", type=float, default=75.0)
    parser.add_argument("--report-dir", default=str(DEFAULT_REPORT_DIR))
    parser.add_argument("--no-report-files", action="store_true")
    args = parser.parse_args(argv)

    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    output_path = Path(args.output) if args.output else DEFAULT_REPORT_DIR / f"soak_{stamp}.csv"
    output_path.parent.mkdir(parents=True, exist_ok=True)

    client = DeviceClient.from_args(args)
    try:
        login_with_retry(client, args.login_timeout_sec)
    except DeviceClientError as exc:
        sys.exit(f"login failed: {exc}")

    started_at = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    interrupted = {"stop": False}

    def _handle_sigint(*_):
        interrupted["stop"] = True
        print("\n[interrupt received — finishing current sample]", file=sys.stderr)

    signal.signal(signal.SIGINT, _handle_sigint)

    samples: list[dict] = []
    deadline = time.monotonic() + args.duration if args.duration > 0 else None

    print(
        f"polling {client.base_url} every {args.interval:.0f}s, "
        f"duration={'infinite' if deadline is None else f'{args.duration:.0f}s'}, "
        f"output={output_path}"
    )

    with output_path.open("w", newline="") as fh:
        writer = csv.DictWriter(fh, fieldnames=CSV_FIELDS)
        writer.writeheader()

        while not interrupted["stop"]:
            sample_start = time.monotonic()
            info = get_with_auth(client, "/api/system/info")
            tasks = get_with_auth(client, "/api/system/tasks?details=1")
            summary = get_with_auth(client, "/api/diagnostics/summary")
            mutexes = get_with_auth(client, "/api/diagnostics/mutexes")
            sample = extract_sample(info, tasks, summary, mutexes)
            sample["sample_latency_ms"] = round((time.monotonic() - sample_start) * 1000, 2)
            sample["sample_errors"] = sum(1 for value in (info, tasks, summary, mutexes) if value is None)
            samples.append(sample)
            writer.writerow(sample)
            fh.flush()

            print(
                f"  {sample['timestamp_iso']} "
                f"free={sample['free_heap']} "
                f"min_free={sample['min_free_heap']} "
                f"psram={sample['free_psram']} "
                f"wm={sample['min_stack_watermark']} ({sample['min_stack_task_name']}) "
                f"wsDrops={sample['ws_queue_drops']} "
                f"lockTimeouts={sample['lock_standard_timeouts']}/{sample['lock_recursive_timeouts']}"
            )

            if deadline is not None and time.monotonic() >= deadline:
                break

            elapsed = time.monotonic() - sample_start
            sleep_for = max(0.0, args.interval - elapsed)
            # Sleep in 1 s slices so SIGINT cuts in quickly.
            slept = 0.0
            while slept < sleep_for and not interrupted["stop"]:
                step = min(1.0, sleep_for - slept)
                time.sleep(step)
                slept += step

    if args.plot:
        maybe_plot(samples, output_path)

    exit_code = summarize(samples, args.regression_threshold)
    finished_at = datetime.datetime.now(datetime.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    report = build_report(
        target=client.base_url,
        started_at=started_at,
        finished_at=finished_at,
        output_path=output_path,
        samples=samples,
        exit_code=exit_code,
    )
    if not args.no_report_files:
        paths = write_report(report, Path(args.report_dir))
        print(f"JSON report: {paths['json']}")
        print(f"Markdown report: {paths['markdown']}")

    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
