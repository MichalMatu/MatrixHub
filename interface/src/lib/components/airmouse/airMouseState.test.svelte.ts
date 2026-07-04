import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useAirMouseManagement } from './airMouseState.svelte';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';

const { mockApi } = vi.hoisted(() => ({
	mockApi: {
		getStatus: vi.fn(),
		updateConfig: vi.fn(),
		calibrate: vi.fn(),
		listScripts: vi.fn(),
		getSettings: vi.fn()
	}
}));

const { mockCreateService } = vi.hoisted(() => ({
	mockCreateService: vi.fn()
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown error';
	})
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$app/state', () => ({
	page: {
		data: {
			features: {
				security: false
			}
		}
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: {
		bearer_token: 'test-token'
	}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: {
		success: vi.fn(),
		error: vi.fn(),
		warning: vi.fn()
	}
}));

function createMockConnection() {
	return {
		wsConnected: false,
		deltaGHistory: [],
		gyroXHistory: [],
		gyroZHistory: [],
		maxHistory: 100,
		uiRequestingConnection: false,
		init() {},
		destroy() {},
		updateConnectionState() {},
		stopWebSocket() {},
		resetHistory() {}
	};
}

describe('useAirMouseManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockCreateService.mockReturnValue(mockApi);
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.useRealTimers();
	});

	it('loads status and scripts', async () => {
		const mockStatus = {
			calibrating: true,
			jiggler: { mode: 0, interval: 60, move_distance: 1, random_interval: false },
			imu: { gx: 0, gy: 0, gz: 0, ax: 0, ay: 0, az: 0 },
			last_delta_g: 0
		} as unknown as AirMouseStatus;

		mockApi.getStatus.mockResolvedValue(mockStatus);
		mockApi.listScripts.mockResolvedValue([]);
		mockApi.getSettings.mockResolvedValue({ enabled: false, boot_script: '', boot_delay: 0 });

		await new Promise<void>((resolve) => {
			let cleanup: (() => void) | undefined;
			cleanup = $effect.root(() => {
				const mouseState = useAirMouseManagement({
					createConnection: () => createMockConnection(),
					shouldInit: () => false
				});

				void mouseState.fetchStatus(true).then(async () => {
					expect(mockApi.getStatus).toHaveBeenCalled();
					expect(mouseState.status).toEqual(mockStatus);
					await vi.waitFor(() => {
						expect(mouseState.macrosEnabled).toBe(false);
					});
					expect(mouseState.macrosEnabled).toBe(false);
					expect(mouseState.calibrating).toBe(true);
					expect(mouseState.loading).toBe(false);
					expect(mouseState.error).toBe(null);
					cleanup?.();
					resolve();
				});
			});
		});
	});

	it('keeps the AirMouse screen usable when macro metadata times out', async () => {
		const mockStatus = {
			calibrating: false,
			jiggler: { mode: 0, interval: 60, move_distance: 1, random_interval: false },
			imu: { gx: 0, gy: 0, gz: 0, ax: 0, ay: 0, az: 0 },
			last_delta_g: 0
		} as unknown as AirMouseStatus;

		mockApi.getStatus.mockResolvedValue(mockStatus);
		mockApi.listScripts.mockRejectedValue(new Error('Timed out'));
		mockApi.getSettings.mockRejectedValue(new Error('Timed out'));

		await new Promise<void>((resolve) => {
			let cleanup: (() => void) | undefined;
			cleanup = $effect.root(() => {
				const mouseState = useAirMouseManagement({
					createConnection: () => createMockConnection(),
					shouldInit: () => false
				});

				void mouseState.fetchStatus(true).then(async () => {
					await Promise.resolve();
					expect(mouseState.status).toEqual(mockStatus);
					expect(mouseState.error).toBe(null);
					expect(mouseState.loading).toBe(false);
					expect(mouseState.scripts).toEqual([]);
					expect(mouseState.macrosEnabled).toBe(null);
					cleanup?.();
					resolve();
				});
			});
		});
	});

	it('clears status and scripts when loading fails', async () => {
		mockApi.getStatus.mockRejectedValue(new Error('Network error'));
		mockApi.listScripts.mockResolvedValue([{ name: 'macro.txt' }]);
		mockApi.getSettings.mockResolvedValue({ enabled: true, boot_script: '', boot_delay: 0 });

		await new Promise<void>((resolve) => {
			let cleanup: (() => void) | undefined;
			cleanup = $effect.root(() => {
				const mouseState = useAirMouseManagement({
					createConnection: () => createMockConnection(),
					shouldInit: () => false
				});

				void mouseState.fetchStatus(true).then(() => {
					expect(mouseState.error).toBeTruthy();
					expect(mouseState.loading).toBe(false);
					expect(mouseState.status).toBe(null);
					expect(mouseState.scripts).toEqual([]);
					cleanup?.();
					resolve();
				});
			});
		});
	});

	it('refreshes status after successful calibration', async () => {
		mockApi.calibrate.mockResolvedValue({});
		mockApi.getStatus.mockResolvedValue({
			jiggler: { mode: 0, interval: 60, move_distance: 1, random_interval: false },
			imu: { gx: 0, gy: 0, gz: 0, ax: 0, ay: 0, az: 0 },
			last_delta_g: 0
		});
		mockApi.listScripts.mockResolvedValue([]);
		mockApi.getSettings.mockResolvedValue({ enabled: true, boot_script: '', boot_delay: 0 });
		vi.useFakeTimers();

		await new Promise<void>((resolve) => {
			let cleanup: (() => void) | undefined;
			cleanup = $effect.root(() => {
				const mouseState = useAirMouseManagement({
					createConnection: () => createMockConnection(),
					shouldInit: () => false
				});

				void mouseState.calibrate().then(async () => {
					expect(mockApi.calibrate).toHaveBeenCalled();
					await vi.advanceTimersByTimeAsync(2000);
					await Promise.resolve();
					expect(mockApi.getStatus).toHaveBeenCalled();
					cleanup?.();
					resolve();
				});
			});
		});
	});

	it('saves updated settings payload', async () => {
		mockApi.updateConfig.mockResolvedValue({});

		await new Promise<void>((resolve) => {
			let cleanup: (() => void) | undefined;
			cleanup = $effect.root(() => {
				const mouseState = useAirMouseManagement({
					createConnection: () => createMockConnection(),
					shouldInit: () => false
				});

				mouseState.status = {
					jiggler: { mode: 0, interval: 60, move_distance: 1, random_interval: false },
					imu: { gx: 0, gy: 0, gz: 0, ax: 0, ay: 0, az: 0 },
					last_delta_g: 0
				} as unknown as AirMouseStatus;

				void mouseState
					.saveSettings({
						jiggler: {
							mode: 1,
							interval: 60,
							move_distance: 1,
							random_interval: false
						}
					})
					.then(() => {
						expect(mockApi.updateConfig).toHaveBeenCalledWith(
							expect.objectContaining({
								jiggler: {
									mode: 1,
									interval: 60,
									move_distance: 1,
									random_interval: false
								}
							})
						);
						cleanup?.();
						resolve();
					});
			});
		});
	});

	it('does not reinitialize repeatedly when status updates after mount', async () => {
		const mockStatus = {
			calibrating: false,
			movement_enabled: true,
			click_enabled: false,
			click_source: 0,
			jiggler: { mode: 0, interval: 60, move_distance: 1, random_interval: false },
			imu: { gx: 0, gy: 0, gz: 0, ax: 0, ay: 0, az: 0 },
			last_delta_g: 0
		} as unknown as AirMouseStatus;

		mockApi.getStatus.mockResolvedValue(mockStatus);
		mockApi.listScripts.mockResolvedValue([]);
		mockApi.getSettings.mockResolvedValue({ enabled: false, boot_script: '', boot_delay: 0 });

		const connection = {
			...createMockConnection(),
			init: vi.fn(),
			destroy: vi.fn()
		};

		let cleanup: (() => void) | undefined;
		cleanup = $effect.root(() => {
			useAirMouseManagement({
				createConnection: () => connection,
				shouldInit: () => true
			});
		});

		await vi.waitFor(() => {
			expect(mockApi.getStatus).toHaveBeenCalledTimes(1);
		});

		expect(connection.init).toHaveBeenCalledTimes(1);
		expect(connection.destroy).not.toHaveBeenCalled();

		cleanup?.();
	});
});
