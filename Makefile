SHELL := /usr/bin/env bash
.DEFAULT_GOAL := help

INTERFACE_DIR ?= interface
NPM ?= npm
VENV_DIR ?= .venv
PYTHON ?= $(VENV_DIR)/bin/python
PYTHON_REQUIREMENTS ?= requirements.txt

PIO ?= $(shell command -v pio 2>/dev/null || printf '%s' "$$HOME/.platformio/penv/bin/pio")
PIO_ENV ?= waveshare_esp32s3_matrix
PIO_DEV_ENV ?= waveshare_esp32s3_matrix_dev
PIO_JOBS ?= 4

DEVICE_URL ?= $(shell PYTHONPATH=scripts "$(PYTHON)" -c 'from device_client import DEFAULT_DEVICE_URL; print(DEFAULT_DEVICE_URL)' 2>/dev/null || printf 'https://192.168.0.18')
DEVICE_USER ?= admin
DEVICE_PASSWORD ?= admin
VITE_HOST ?= 0.0.0.0
VITE_PORT ?= 5173
E2E_HOST ?= 127.0.0.1
E2E_PORT ?= 5174
DURATION ?= 60

.PHONY: help config python-deps start dev frontend-start frontend-install frontend-build frontend-check frontend-lint frontend-test frontend-test-watch frontend-quality frontend-quality-fast frontend-e2e build-fast build build-release clean explain upload upload-fast monitor stop-monitor test test-native coverage-native diagnostics smoke smoke-safe csi-monitor csi-analyze status

help: ## Show available targets and defaults.
	@awk 'BEGIN {FS = ":.*##"; printf "\nMatrixHub make targets\n\n"} /^[a-zA-Z0-9_.-]+:.*##/ {printf "  %-24s %s\n", $$1, $$2} END {printf "\nDefaults:\n  DEVICE_URL=%s\n  VITE_HOST=%s\n  VITE_PORT=%s\n  PIO_ENV=%s\n\nExamples:\n  make start\n  make start DEVICE_URL=https://192.168.0.18 VITE_HOST=0.0.0.0\n  make build-fast\n  make upload-fast\n\n", "$(DEVICE_URL)", "$(VITE_HOST)", "$(VITE_PORT)", "$(PIO_ENV)"}' $(MAKEFILE_LIST)

config: ## Print effective tool configuration.
	@printf 'DEVICE_URL=%s\n' "$(DEVICE_URL)"
	@printf 'DEVICE_USER=%s\n' "$(DEVICE_USER)"
	@printf 'VITE_HOST=%s\n' "$(VITE_HOST)"
	@printf 'VITE_PORT=%s\n' "$(VITE_PORT)"
	@printf 'E2E_HOST=%s\n' "$(E2E_HOST)"
	@printf 'E2E_PORT=%s\n' "$(E2E_PORT)"
	@printf 'PIO=%s\n' "$(PIO)"
	@printf 'PIO_ENV=%s\n' "$(PIO_ENV)"
	@printf 'PIO_DEV_ENV=%s\n' "$(PIO_DEV_ENV)"
	@printf 'PIO_JOBS=%s\n' "$(PIO_JOBS)"
	@printf 'PYTHON=%s\n' "$(PYTHON)"

python-deps: ## Install Python dependencies for host diagnostics.
	@if [ ! -x "$(PYTHON)" ]; then python3 -m venv "$(VENV_DIR)"; fi
	@"$(PYTHON)" -m pip install -r "$(PYTHON_REQUIREMENTS)"

start: ## Start the SvelteKit/Vite frontend proxied to DEVICE_URL.
	@printf 'Starting MatrixHub UI on http://%s:%s with proxy target %s\n' "$(VITE_HOST)" "$(VITE_PORT)" "$(DEVICE_URL)"
	@cd "$(INTERFACE_DIR)" && DEVICE_URL="$(DEVICE_URL)" VITE_PROXY_TARGET="$(DEVICE_URL)" VITE_DEV_HOST="$(VITE_HOST)" "$(NPM)" run i18n:build
	@cd "$(INTERFACE_DIR)" && DEVICE_URL="$(DEVICE_URL)" VITE_PROXY_TARGET="$(DEVICE_URL)" VITE_DEV_HOST="$(VITE_HOST)" "$(NPM)" exec vite -- dev --host "$(VITE_HOST)" --port "$(VITE_PORT)"

dev: start ## Alias for start.

frontend-start: start ## Alias for start.

frontend-install: ## Install frontend dependencies with npm ci.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" ci

frontend-build: ## Build the SvelteKit frontend.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run build

frontend-check: ## Run Svelte diagnostics.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run check

frontend-lint: ## Run frontend lint and formatting checks.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run lint

frontend-test: ## Run frontend unit tests once.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run test:run

frontend-test-watch: ## Run frontend unit tests in watch mode.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run test

frontend-quality-fast: ## Run lint, check, unit tests, and unused-code checks.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run quality:frontend:fast

frontend-quality: ## Run the full frontend quality gate.
	@"$(NPM)" --prefix "$(INTERFACE_DIR)" run quality:frontend

frontend-e2e: ## Run Playwright E2E against DEVICE_URL.
	@DEVICE_URL="$(DEVICE_URL)" TEST_USERNAME="$(DEVICE_USER)" TEST_PASSWORD="$(DEVICE_PASSWORD)" E2E_HOST="$(E2E_HOST)" E2E_PORT="$(E2E_PORT)" "$(NPM)" --prefix "$(INTERFACE_DIR)" run test:e2e

build-fast: ## Fast firmware build using the dev env and skipping UI.
	@PIO_ENV="$(PIO_DEV_ENV)" PIO_JOBS="$(PIO_JOBS)" ./scripts/build-fast.sh

build: ## Build release firmware with embedded UI.
	@"$(PIO)" run -e "$(PIO_ENV)"

build-release: build ## Alias for build.

clean: ## Clean the main PlatformIO environment.
	@PIO_ENV="$(PIO_ENV)" ./scripts/build-clean.sh

explain: ## Run SCons rebuild diagnosis into build-explain.log.
	@PIO_ENV="$(PIO_ENV)" ./scripts/build-explain.sh

upload: stop-monitor ## Build and upload release firmware.
	@"$(PIO)" run -e "$(PIO_ENV)" -t upload

upload-fast: stop-monitor ## Upload firmware while skipping UI rebuild.
	@SKIP_UI=1 "$(PIO)" run -e "$(PIO_ENV)" -t upload

monitor: ## Start PlatformIO serial monitor.
	@"$(PIO)" device monitor

stop-monitor: ## Stop any active PlatformIO monitor.
	@pkill -f "[p]io device monitor" 2>/dev/null || true

test: test-native ## Alias for test-native.

test-native: ## Run host-side native Unity tests.
	@"$(PIO)" test -e native

coverage-native: ## Run native coverage environment.
	@"$(PIO)" test -e native_coverage

diagnostics: python-deps ## Run runtime diagnostics against DEVICE_URL.
	@DEVICE_URL="$(DEVICE_URL)" DEVICE_USER="$(DEVICE_USER)" DEVICE_PASSWORD="$(DEVICE_PASSWORD)" "$(PYTHON)" scripts/diagnostics/check_runtime_diagnostics.py

smoke: python-deps ## Run read-only device smoke tests against DEVICE_URL.
	@DEVICE_URL="$(DEVICE_URL)" DEVICE_USER="$(DEVICE_USER)" DEVICE_PASSWORD="$(DEVICE_PASSWORD)" "$(PYTHON)" scripts/tests/device_smoke.py --read-only

smoke-safe: python-deps ## Run safe-write device smoke tests against DEVICE_URL.
	@DEVICE_URL="$(DEVICE_URL)" DEVICE_USER="$(DEVICE_USER)" DEVICE_PASSWORD="$(DEVICE_PASSWORD)" "$(PYTHON)" scripts/tests/device_smoke.py --safe-writes

csi-monitor: python-deps ## Monitor CSI data for DURATION seconds.
	@DEVICE_URL="$(DEVICE_URL)" DEVICE_USER="$(DEVICE_USER)" DEVICE_PASSWORD="$(DEVICE_PASSWORD)" "$(PYTHON)" scripts/csi_monitor.py --duration "$(DURATION)"

csi-analyze: python-deps ## Analyze CSI data for DURATION seconds.
	@DEVICE_URL="$(DEVICE_URL)" DEVICE_USER="$(DEVICE_USER)" DEVICE_PASSWORD="$(DEVICE_PASSWORD)" "$(PYTHON)" scripts/analyze_csi.py --duration "$(DURATION)"

status: ## Show git status.
	@git status --short
