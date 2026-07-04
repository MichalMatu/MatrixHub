# MatrixHub Documentation

This directory keeps the maintained documentation for MatrixHub. It is
intentionally small: dated implementation prompts, one-off validation reports,
diagnostic captures, and screenshot-heavy walkthroughs should live outside
`docs/` unless they are promoted into a maintained reference.

## Start Here

- [Engineering reference](engineering/README.md) - firmware architecture,
  operations, integrations, and validation gates.
- [Product readiness](product-readiness.md) - product positioning, sales-facing
  readiness work, credential decision, and preproduction checklist.
- [Agent workflows](agent-workflows/README.md) - task-focused workflows linked
  from the root `AGENTS.md`.
- [User guide](user-guide/README.md) - current UI route map and short operator
  notes without static screenshot walkthroughs.
- [Build speed report](main_docs/BUILD_SPEED_OPTIMIZATION.md) - measured
  PlatformIO build optimization notes for this Raspberry Pi 5 host.

## Preserved Build Assets

The following archives are intentionally kept in `docs/` because they were
provided as build/device support material:

- `esp32s3_8x8_Matrix_server_tls.zip`
- `tools.zip`

Do not delete or rewrite them during documentation cleanup unless the build and
device provisioning flow has been updated to use a different source.

## Future Work

- Improve Wi-Fi CSI matrix visualization only if it becomes a product priority.
  The current 8x8 display path is an MVP. A stronger future design should use a
  display-only signal pipeline: fast EMA versus slow EMA, short median/Hampel
  filtering for impulse noise, top-percentile clipping for static CSI peaks,
  and persistence over several frames so small fast jitter is muted while
  larger sustained room changes become visible. Keep this separate from CSI
  alarm calibration and `wifi_csi_motion`.

## Maintenance Rules

- Keep documentation tied to current code paths and commands.
- Do not commit generated reports, Playwright traces, one-off patches, or dated
  diagnostics as documentation.
- Prefer one concise maintained page over many stale pages.
- If a doc mentions an endpoint, keep `docs/engineering/api-contract.json` and
  `scripts/contract/verify_api_contract.py` in sync.
