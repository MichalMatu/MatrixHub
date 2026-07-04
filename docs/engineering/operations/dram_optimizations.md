# DRAM And PSRAM Guidance

Navigation: [Project README](../../../README.md) - [Engineering Reference](../README.md)

This guide describes the current memory rules for MatrixHub on ESP32-S3.

## Main Rule

Internal DRAM is scarce. Use PSRAM for large data that does not need DMA, ISR
access, or very low latency.

Prefer PSRAM for:

- HTTP response buffers over roughly 512 bytes
- JSON documents for API responses and config payloads
- notification queues and background worker buffers
- diagnostic snapshots and file-transfer buffers
- long-lived feature state that is not touched by ISRs or DMA

Keep internal DRAM for:

- DMA buffers
- ISR-accessed queues or fields
- small latency-critical task state
- stacks that must stay internal for platform/runtime reasons

## JSON

Use `SYSTEM::SpiRamJsonDocument` for larger JSON documents. Avoid building large
responses as:

```cpp
String json;
serializeJson(doc, json);
```

Prefer existing streaming helpers such as:

- `SYSTEM::JsonResponseWriter`
- `PsychicStreamResponse`
- response helpers already used by nearby API modules

If a response is large or hot-path, stream it once instead of building a DOM,
measuring it, and serializing again.

## Allocation

Use `heap_caps_malloc(..., MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)` for large
general-purpose buffers.

Always free matching allocations with `heap_caps_free()`.

Avoid long-lived Arduino `String` objects in services. Prefer fixed buffers,
`std::string` with project allocators, or existing PSRAM helpers.

## Task Stacks

Task stack sizes live in `CONFIG::TASKS` in `src/config/TaskConfig.h`.

Do not lower stack sizes from intuition. Use runtime high-water marks and logs
first, then reduce one task at a time with a focused test.

When adding a task:

1. define stack, priority, and core in `CONFIG::TASKS`
2. document why the core and priority are chosen if the task is latency-sensitive
3. log or inspect stack high-water marks during validation

## ISR And DMA Boundaries

Callbacks running from ISR context must not allocate, block, or take normal
mutexes. Pass fixed-size packets to a worker task through ISR-safe queues.

For CSI, keep `wifi_csi_rx_cb` minimal and use the queue path under
`src/wifisensing/csi/data/`.

For matrix/LED/DMA paths, verify buffer capabilities before moving memory to
PSRAM.

## Validation

Useful checks:

```bash
pio test -e native
./scripts/build-fast.sh
python scripts/diagnostics/check_runtime_diagnostics.py --device-url https://192.168.0.30 --user admin --password admin --json
```

For long-running memory work, use the scripts under `scripts/tests/` and store
raw artifacts under `artifacts/`, not in `docs/`.
