import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { ApiError } from '$lib/utils';

const TEST_IN_PROGRESS_ERROR = 'busy/test_in_progress';
const TEST_BUTTON_COOLDOWN_MS = 4000;

class TestExecutionGate {
	private requestInFlight = $state(false);
	private cooldownUntilMs = $state(0);
	private cooldownTimer: ReturnType<typeof setTimeout> | null = null;

	get isBlocked() {
		return this.requestInFlight || this.cooldownUntilMs > Date.now();
	}

	tryBeginRequest(): (() => void) | null {
		this.clearExpiredCooldown();
		if (this.isBlocked) {
			return null;
		}

		this.clearCooldownTimer();
		this.requestInFlight = true;

		return () => {
			this.requestInFlight = false;
			this.startCooldown(TEST_BUTTON_COOLDOWN_MS);
		};
	}

	resetForTests() {
		this.requestInFlight = false;
		this.cooldownUntilMs = 0;
		this.clearCooldownTimer();
	}

	private clearExpiredCooldown() {
		if (this.cooldownUntilMs > 0 && this.cooldownUntilMs <= Date.now()) {
			this.cooldownUntilMs = 0;
		}
	}

	private startCooldown(ms: number) {
		this.clearCooldownTimer();
		if (ms <= 0) {
			this.cooldownUntilMs = 0;
			return;
		}

		this.cooldownUntilMs = Date.now() + ms;
		this.cooldownTimer = setTimeout(() => {
			if (this.cooldownUntilMs <= Date.now()) {
				this.cooldownUntilMs = 0;
			}
			this.cooldownTimer = null;
		}, ms);
	}

	private clearCooldownTimer() {
		if (this.cooldownTimer) {
			clearTimeout(this.cooldownTimer);
			this.cooldownTimer = null;
		}
	}
}

const gate = new TestExecutionGate();

export function getTestExecutionGate() {
	return gate;
}

function getTestInProgressMessage(): string {
	return m.toast_test_in_progress({ locale: i18n.languageTag });
}

export function mapTestExecutionError(error: unknown): string | null {
	if (typeof error === 'string' && error === TEST_IN_PROGRESS_ERROR) {
		return getTestInProgressMessage();
	}

	if (error instanceof ApiError && error.status === 429) {
		return getTestInProgressMessage();
	}

	if (error instanceof Error && error.message === TEST_IN_PROGRESS_ERROR) {
		return getTestInProgressMessage();
	}

	return null;
}

export function resetGlobalTestExecutionGateForTests() {
	gate.resetForTests();
}
