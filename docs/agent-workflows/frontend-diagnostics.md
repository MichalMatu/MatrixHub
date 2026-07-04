# Frontend Diagnostics Workflow

Run commands from `interface/`.

## Fast Gate

```bash
npm run quality:frontend:fast
```

This runs lint, Svelte diagnostics, Vitest, and unused-code checks.

## Full Gate

```bash
npm run quality:frontend
```

This adds dependency-cruiser, production build, and size-budget checks.

## E2E Gate

Use when touching navigation, settings save flows, accessibility, responsive
layout, auth shell, or Matrix LED:

```bash
npm run quality:frontend:e2e
```

For a full Playwright run against a device:

```bash
DEVICE_URL=https://192.168.0.30 TEST_USERNAME=admin TEST_PASSWORD=admin npm run test:e2e
```
