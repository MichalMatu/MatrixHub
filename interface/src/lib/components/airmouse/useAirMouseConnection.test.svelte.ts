import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { SystemEvent } from '$lib/stores/system/types';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { AIR_MOUSE_WS } from './airMouseConfig';
import { useAirMouseConnection } from './useAirMouseConnection.svelte';

class MockEventBus {
	private subscribers = new Set<(value: SystemEvent | null) => void>();

	subscribe(run: (value: SystemEvent | null) => void) {
		this.subscribers.add(run);
		run(null);
		return () => {
			this.subscribers.delete(run);
		};
	}

	set(value: SystemEvent | null) {
		this.subscribers.forEach((run) => run(value));
	}
}

function createStatus(): AirMouseStatus {
	return {
		movement_enabled: true,
		click_enabled: false,
		running: true,
		calibrating: false,
		sensitivity_x: 200,
		sensitivity_y: 200,
		deadzone: 2,
		acceleration_enabled: false,
		acceleration_factor: 1,
		tap_threshold_g: 0.4,
		click_debounce_ms: 200,
		double_click_window_ms: 500,
		click_source: 0,
		single_click_action: 1,
		double_click_action: 2,
		triple_click_action: 0,
		single_click_script: '',
		double_click_script: '',
		triple_click_script: '',
		euro_min_cutoff: 0,
		euro_beta: 0,
		euro_d_cutoff: 0,
		gyro_offset_x: 0,
		gyro_offset_y: 0,
		gyro_offset_z: 0,
		last_delta_g: 0,
		jiggler: { mode: 0, interval: 60, move_distance: 1, random_interval: false },
		imu: { gx: 0, gy: 0, gz: 0, ax: 0, ay: 0, az: 0 }
	};
}

describe('useAirMouseConnection', () => {
	beforeEach(() => {
		vi.restoreAllMocks();
		vi.useRealTimers();
	});

	afterEach(() => {
		vi.restoreAllMocks();
	});

	it('subscribes to the shared AirMouse system channel when IMU streaming is needed', () => {
		const eventBus = new MockEventBus();
		const systemStatusStore = {
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn(),
			getSnapshot: vi.fn(() => null),
			requestSnapshot: vi.fn(),
			isConnected: true
		};

		let cleanup: (() => void) | undefined;
		cleanup = $effect.root(() => {
			let status = createStatus();
			const connection = useAirMouseConnection(
				() => status,
				(nextStatus) => {
					status = nextStatus ?? status;
				},
				{
					systemStatusStore,
					systemEventsBus: eventBus
				}
			);

			connection.init();
			connection.uiRequestingConnection = true;
			connection.updateConnectionState();

			expect(systemStatusStore.subscribeChannel).toHaveBeenCalledWith('airmouse');

			connection.destroy();
		});

		cleanup?.();
	});

	it('updates status and history from shared AirMouse telemetry events', () => {
		const eventBus = new MockEventBus();
		const systemStatusStore = {
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn(),
			getSnapshot: vi.fn(() => null),
			requestSnapshot: vi.fn(),
			isConnected: true
		};

		let cleanup: (() => void) | undefined;
		cleanup = $effect.root(() => {
			let status = createStatus();
			const connection = useAirMouseConnection(
				() => status,
				(nextStatus) => {
					status = nextStatus ?? status;
				},
				{
					systemStatusStore,
					systemEventsBus: eventBus
				}
			);

			connection.init();
			connection.uiRequestingConnection = true;
			connection.updateConnectionState();

			eventBus.set({
				type: 'airmouse',
				data: {
					gx: 11,
					gy: 22,
					gz: 33,
					ax: 44,
					ay: 55,
					az: 66,
					deltaG: 1.5
				}
			});

			expect(status.last_delta_g).toBe(1.5);
			expect(status.imu.gx).toBe(11);
			expect(status.imu.gz).toBe(33);
			expect(connection.deltaGHistory).toEqual([1.5]);
			expect(connection.gyroXHistory).toEqual([11]);
			expect(connection.gyroZHistory).toEqual([33]);

			connection.destroy();
		});

		cleanup?.();
	});

	it('refreshes the shared channel subscription when the watchdog expires', async () => {
		vi.useFakeTimers();
		vi.spyOn(console, 'warn').mockImplementation(() => {});

		const eventBus = new MockEventBus();
		const systemStatusStore = {
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn(),
			getSnapshot: vi.fn(() => null),
			requestSnapshot: vi.fn(),
			isConnected: true
		};

		let connection!: ReturnType<typeof useAirMouseConnection>;
		let cleanup: (() => void) | undefined;
		cleanup = $effect.root(() => {
			let status = createStatus();
			connection = useAirMouseConnection(
				() => status,
				(nextStatus) => {
					status = nextStatus ?? status;
				},
				{
					systemStatusStore,
					systemEventsBus: eventBus
				}
			);

			connection.init();
			connection.uiRequestingConnection = true;
			connection.updateConnectionState();
		});

		await vi.advanceTimersByTimeAsync(AIR_MOUSE_WS.watchdogTimeoutMs);

		expect(systemStatusStore.unsubscribeChannel).toHaveBeenCalledWith('airmouse');
		expect(systemStatusStore.subscribeChannel).toHaveBeenCalledTimes(2);
		expect(systemStatusStore.subscribeChannel).toHaveBeenNthCalledWith(2, 'airmouse');

		connection.destroy();
		cleanup?.();
	});

	it('clears the shared watchdog during teardown', async () => {
		vi.useFakeTimers();
		const warnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});

		const eventBus = new MockEventBus();
		const systemStatusStore = {
			subscribeChannel: vi.fn(),
			unsubscribeChannel: vi.fn(),
			getSnapshot: vi.fn(() => null),
			requestSnapshot: vi.fn(),
			isConnected: true
		};

		let connection!: ReturnType<typeof useAirMouseConnection>;
		let cleanup: (() => void) | undefined;
		cleanup = $effect.root(() => {
			let status = createStatus();
			connection = useAirMouseConnection(
				() => status,
				(nextStatus) => {
					status = nextStatus ?? status;
				},
				{
					systemStatusStore,
					systemEventsBus: eventBus
				}
			);

			connection.init();
			connection.uiRequestingConnection = true;
			connection.updateConnectionState();
		});

		connection.destroy();
		await vi.advanceTimersByTimeAsync(AIR_MOUSE_WS.watchdogTimeoutMs);

		expect(warnSpy).not.toHaveBeenCalled();

		cleanup?.();
	});
});
