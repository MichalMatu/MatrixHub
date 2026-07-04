import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useKeyboardManagement } from './useKeyboardManagement.svelte';

vi.mock('$lib/paraglide/messages', () => ({
	keyboard_sent: () => 'sent',
	keyboard_sending: () => 'sending',
	keyboard_network_error: () => 'network error',
	keyboard_error_status: ({ status }: { status: number }) => `status ${status}`
}));

class DummyKeyboardManager {
	keyboard = {
		getInput: () => this.input
	};

	private input = '';

	setMode() {}
	setModifiers() {}
	setCapsLock() {}
	setPressedButtons() {}
	setInput(input: string) {
		this.input = input;
	}
	clearInput() {
		this.input = '';
	}
	destroy() {}
}

describe('useKeyboardManagement', () => {
	beforeEach(() => {
		vi.useFakeTimers();
		vi.spyOn(console, 'error').mockImplementation(() => undefined);
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.useRealTimers();
	});

	it('shows error status when live typing request fails', async () => {
		const api = {
			typeText: vi.fn().mockRejectedValue({ status: 503 }),
			pressKey: vi.fn().mockResolvedValue(undefined)
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const keyboard = useKeyboardManagement(() => '', {
					createApi: () => api,
					createManager: () => new DummyKeyboardManager()
				});

				keyboard.init();
				keyboard.toggleLiveMode();

				void keyboard.sendImmediate('a').then(() => {
					expect(api.typeText).toHaveBeenCalledWith({ text: 'a', enter: false });
					expect(keyboard.lastStatus).toBe('status 503');
					expect(keyboard.lastStatusKind).toBe('error');
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('does not let an older success timer clear a newer error state', async () => {
		const api = {
			typeText: vi.fn().mockResolvedValue(undefined),
			pressKey: vi.fn().mockRejectedValue(new Error('press failed'))
		};

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const keyboard = useKeyboardManagement(() => '', {
					createApi: () => api,
					createManager: () => new DummyKeyboardManager()
				});

				keyboard.init();
				keyboard.toggleLiveMode();

				void keyboard
					.sendImmediate('a')
					.then(() => keyboard.pressDirect(177))
					.then(async () => {
						expect(keyboard.lastStatus).toBe('network error');
						expect(keyboard.lastStatusKind).toBe('error');

						await vi.advanceTimersByTimeAsync(500);

						expect(keyboard.lastStatus).toBe('network error');
						expect(keyboard.lastStatusKind).toBe('error');
						resolve();
					});
			});
		});

		cleanup?.();
	});
});
