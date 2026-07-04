# Frontend Quality Gate

MatrixHub frontend changes should keep the interface shippable for the ESP32/LittleFS target and for normal browser use.

## Commands

Run from `interface/`:

- `npm run quality:frontend:fast` for daily work before handing off a change.
- `npm run quality:frontend` before a PR is ready for review.
- `npm run quality:frontend:e2e` when a change touches navigation, settings save flows, accessibility, responsive layout, auth shell, or Matrix LED.

`quality:frontend` runs lint, Svelte diagnostics, Vitest, Knip, dependency-cruiser, production build, and the build-size budget check.

## PR Rules

- Do not increase `.dependency-cruiser-known-violations.json`. The dependency-cruiser baseline may shrink without discussion; growth needs an explicit architecture note.
- Do not raise size budgets to make a PR pass. If the client JS, CSS, or total build budget is exceeded, reduce code or document the trade-off before changing limits.
- Editable settings cards must use the shared settings-card contract: visible Save, disabled Save without changes, Discard/Reset for drafts, double-click save guard, and an accessible status/error path. Status-only/read-only cards must be explicit in review.
- Keep Playwright traces, videos, and screenshots as failure artifacts only. They are debugging output, not source material.
- For e2e tests without a device, mock only root backend routes that begin with `/api/` or `/rest/`; never intercept source modules that merely contain `/api/` in their path.

## CI Expectations

A frontend CI job should:

1. Install dependencies in `interface/` with `npm ci`.
2. Install Chromium with `npx playwright install --with-deps chromium`.
3. Run `npm run quality:frontend`.
4. Run `npm run quality:frontend:e2e` for PRs that touch frontend behavior.
5. Publish `interface/test-results` and `interface/playwright-report` only when Playwright fails.
