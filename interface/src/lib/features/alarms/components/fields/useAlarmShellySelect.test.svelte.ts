import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { SystemEvent } from '$lib/stores/system/types';

type ShellyState = ReturnType<typeof import('./useAlarmShellySelect.svelte').useAlarmShellySelect>;

const { registerDestroy, runDestroy, captureEventCallback, emitEvent } = vi.hoisted(() => {
	let destroyCallbacks: Array<() => void> = [];
	let eventCallback: ((event: SystemEvent | null) => void) | null = null;

	return {
		registerDestroy: (callback: () => void) => {
			destroyCallbacks.push(callback);
		},
		runDestroy: () => {
			for (const callback of destroyCallbacks) {
				callback();
			}
			destroyCallbacks = [];
			eventCallback = null;
		},
		captureEventCallback: (callback: (event: SystemEvent | null) => void) => {
			eventCallback = callback;
		},
		emitEvent: (event: SystemEvent | null) => {
			eventCallback?.(event);
		}
	};
});

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal<typeof import('svelte')>();
	return {
		...actual,
		onMount: (fn: () => void) => fn(),
		onDestroy: (fn: () => void) => registerDestroy(fn)
	};
});

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	toast_request_timeout: () => 'Request timeout',
	toast_shelly_device_limit: () => 'Maximum 4 Shelly devices per rule allowed.',
	toast_message: ({ message }: { message: string }) => `toast: ${message}`
}));

describe('useAlarmShellySelect', () => {
	beforeEach(() => {
		runDestroy();
		vi.clearAllMocks();
		vi.useRealTimers();
	});

	it('uses cached snapshot and blocks adding more than four Shelly devices', async () => {
		const { useAlarmShellySelect } = await import('./useAlarmShellySelect.svelte');
		const notifications = {
			warning: vi.fn()
		};
		const statusStore = {
			getSnapshot: vi.fn().mockReturnValue([{ id: 'relay-1', name: 'Desk plug' }]),
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn()
		};
		const unsubscribe = vi.fn();
		const eventBus = {
			subscribe: vi.fn((callback: (event: SystemEvent | null) => void) => {
				captureEventCallback(callback);
				return unsubscribe;
			})
		};

		let selectedIds = ['a', 'b', 'c', 'd'];
		let shellyState!: ShellyState;
		const cleanup = $effect.root(() => {
			shellyState = useAlarmShellySelect(
				() => selectedIds,
				(ids) => {
					selectedIds = ids;
				},
				{
					statusStore,
					eventBus,
					notifications
				}
			);
		});

		expect(statusStore.subscribeChannel).toHaveBeenCalledWith('shelly');
		expect(shellyState.loading).toBe(false);
		expect(shellyState.error).toBeNull();
		expect(shellyState.devices).toEqual([{ id: 'relay-1', name: 'Desk plug' }]);

		shellyState.toggleDevice('relay-2');

		expect(notifications.warning).toHaveBeenCalledWith(
			'toast: Maximum 4 Shelly devices per rule allowed.',
			3000
		);
		expect(selectedIds).toEqual(['a', 'b', 'c', 'd']);

		runDestroy();
		expect(statusStore.unsubscribeChannel).toHaveBeenCalledWith('shelly');
		expect(unsubscribe).toHaveBeenCalledOnce();
		cleanup();
	});

	it('times out waiting for Shelly snapshot and recovers after a later snapshot event', async () => {
		vi.useFakeTimers();
		const { useAlarmShellySelect } = await import('./useAlarmShellySelect.svelte');
		const statusStore = {
			getSnapshot: vi.fn().mockReturnValue(null),
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn()
		};
		const eventBus = {
			subscribe: vi.fn((callback: (event: SystemEvent | null) => void) => {
				captureEventCallback(callback);
				return vi.fn();
			})
		};

		let shellyState!: ShellyState;
		const cleanup = $effect.root(() => {
			shellyState = useAlarmShellySelect(
				() => [],
				() => {},
				{
					statusStore,
					eventBus,
					timeoutMs: 5000
				}
			);
		});

		expect(shellyState.loading).toBe(true);

		await vi.advanceTimersByTimeAsync(5000);

		expect(shellyState.loading).toBe(false);
		expect(shellyState.error).toBe('Request timeout');

		emitEvent({
			type: 'snapshot',
			channel: 'shelly',
			data: [{ id: 'relay-3', name: 'Hallway plug' }]
		});

		expect(shellyState.error).toBeNull();
		expect(shellyState.devices).toEqual([{ id: 'relay-3', name: 'Hallway plug' }]);

		runDestroy();
		cleanup();
	});
});
