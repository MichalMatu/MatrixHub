import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { GpioConfig, GpioStatus } from '$lib/types/domain/gpio';

const { mockNotifications, mockLogger } = vi.hoisted(() => ({
	mockNotifications: {
		success: vi.fn(),
		error: vi.fn()
	},
	mockLogger: {
		error: vi.fn()
	}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown';
	})
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	gpio_error_load: () => 'GPIO load error',
	gpio_error_save: () => 'GPIO save error',
	gpio_error_output: () => 'GPIO output error',
	gpio_saved: () => 'GPIO saved',
	toast_message: ({ message }: { message: string }) => message
}));

function createConfig(): GpioConfig {
	return {
		channels: [
			{
				id: 'gpio1',
				name: 'GPIO 1',
				pin: 1,
				mode: 'input',
				pull: 'none',
				inverted: false,
				debounce_ms: 50,
				initial_output: false
			}
		]
	};
}

function createStatus(): GpioStatus {
	return {
		channels: [
			{
				id: 'gpio1',
				name: 'GPIO 1',
				pin: 1,
				mode: 'input',
				configured: true,
				raw: false,
				logical: false,
				stable: true,
				sampled_at: 10,
				changed_at: 10
			}
		]
	};
}

function deferred<T>() {
	let resolve!: (value: T) => void;
	let reject!: (reason?: unknown) => void;
	const promise = new Promise<T>((res, rej) => {
		resolve = res;
		reject = rej;
	});
	return { promise, resolve, reject };
}

describe('useGpioManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('renders config without waiting for slow status and pins requests', async () => {
		const { useGpioManagement } = await import('./useGpioManagement.svelte');
		const status = deferred<GpioStatus>();
		const pins = deferred<never[]>();
		const api = {
			getPins: vi.fn(() => pins.promise),
			getConfig: vi.fn().mockResolvedValue(createConfig()),
			getStatus: vi.fn(() => status.promise),
			saveConfig: vi.fn(),
			setOutput: vi.fn()
		};

		let gpio!: ReturnType<typeof useGpioManagement>;
		const cleanup = $effect.root(() => {
			gpio = useGpioManagement({
				api: api as never,
				session: { canRead: true, canManage: true },
				notifications: mockNotifications,
				logger: mockLogger
			});
		});

		await gpio.load();

		expect(api.getConfig).toHaveBeenCalledOnce();
		expect(api.getPins).toHaveBeenCalledOnce();
		expect(api.getStatus).toHaveBeenCalledOnce();
		expect(gpio.loading).toBe(false);
		expect(gpio.config?.channels[0]?.id).toBe('gpio1');
		expect(gpio.status).toEqual([]);

		pins.resolve([]);
		status.resolve(createStatus());
		await vi.waitFor(() => {
			expect(gpio.status).toHaveLength(1);
		});

		cleanup();
	});

	it('does not overlap status refreshes while one is still in flight', async () => {
		const { useGpioManagement } = await import('./useGpioManagement.svelte');
		const status = deferred<GpioStatus>();
		const api = {
			getPins: vi.fn(),
			getConfig: vi.fn(),
			getStatus: vi.fn(() => status.promise),
			saveConfig: vi.fn(),
			setOutput: vi.fn()
		};

		let gpio!: ReturnType<typeof useGpioManagement>;
		const cleanup = $effect.root(() => {
			gpio = useGpioManagement({
				api: api as never,
				session: { canRead: true, canManage: true },
				notifications: mockNotifications,
				logger: mockLogger
			});
		});

		const first = gpio.refreshStatus();
		await Promise.resolve();
		const second = gpio.refreshStatus();

		expect(api.getStatus).toHaveBeenCalledOnce();
		status.resolve(createStatus());
		await Promise.all([first, second]);
		expect(gpio.status).toHaveLength(1);

		cleanup();
	});
});
