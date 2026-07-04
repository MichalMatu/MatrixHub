# API Contract

Navigation: [Engineering Reference](README.md)

`api-contract.json` is the local source of truth for firmware, frontend, SDK,
diagnostic scripts, and documentation endpoint paths.

The contract intentionally tracks:

- method and path
- authentication level
- high-level request and response shape
- whether the operation can restart, sleep, or reset the device
- firmware implementation files
- frontend, SDK, and diagnostic coverage

## Validation

Run this after changing any endpoint path, frontend API service, SDK method,
diagnostic script, or user guide that references `/api/*`, `/rest/*`, or
`/ws/*`:

```bash
python scripts/contract/verify_api_contract.py
```

The verifier scans:

- `src/api/**`, selected firmware config constants, and `lib/framework/**`
- `interface/src/lib/services/api/**`
- `interface/src/lib/services/fileManager/**`
- `interface/src/lib/stores/**`
- `interface/src/lib/features/**`
- `packages/device-sdk/src/**`
- `scripts/**` and `tools/**`
- `docs/user-guide/**`

It fails when a local API path literal exists in code or maintained docs but is
missing from `api-contract.json`.

## Runtime Rules

- Device default URL: `https://192.168.0.30`
- mDNS alias: `https://matrixhub.local`
- Plain HTTP is only a port-80 convenience redirect to HTTPS. It must not serve
  API or UI content directly.
- Diagnostic scripts should use `scripts/device_client.py` for HTTPS, JWT, TLS
  verification policy, retries, and WebSocket cookie auth.
- WebSocket JWT auth uses an `access_token` cookie. Do not use
  `access_token=...` query parameters.
- `/ws/csi` payloads are binary and must stay aligned with
  `src/api/wifisensing/CsiWireFormat.cpp`,
  `interface/src/lib/features/wifisensing/csi/parseCsiFrame.ts`, and
  `docs/engineering/integrations/csi.md`.

## Required Gates For Contract Changes

Run the focused contract gate first:

```bash
python scripts/contract/verify_api_contract.py
python -m py_compile scripts/device_client.py scripts/analyze_csi.py scripts/csi_monitor.py scripts/sensing_analysis/collect_long_data.py scripts/diagnostics/check_runtime_diagnostics.py tools/csi_client.py scripts/contract/verify_api_contract.py
```

Then run the normal MatrixHub quality gates:

```bash
/home/test/.platformio/penv/bin/pio test -e native
cd interface && npm run check
cd interface && npm run test:run
cd interface && DEVICE_URL=https://192.168.0.30 TEST_USERNAME=admin TEST_PASSWORD=admin npm run test:e2e
```

## Release Contract Checklist

For a release candidate, run the contract verifier even when no endpoint paths
were intentionally changed:

```bash
python scripts/contract/verify_api_contract.py
python scripts/diagnostics/check_runtime_diagnostics.py --device-url https://192.168.0.30 --username admin --password admin
python scripts/tests/device_smoke.py --device-url https://192.168.0.30 --username admin --password admin --safe-writes
```

The release contract is healthy when:

- all maintained `/api/*`, `/rest/*`, and `/ws/*` literals are represented in
  `api-contract.json`,
- firmware, frontend services, SDK, diagnostics, and docs agree on HTTPS/WSS
  paths,
- WebSockets authenticate with the `access_token` cookie,
- diagnostics endpoints expose heap, task, mutex, feature, and endpoint data,
- smoke reports no missing endpoints, schema mismatches, or unexpected restarts.

Notification delivery test endpoints are intentionally excluded from the shared
device release checklist. Keep them covered by local unit tests and only run
live delivery checks on an isolated device/account.
