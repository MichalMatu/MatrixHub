import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useShellyDevices } from './useShellyDevices.svelte';

const { mockApi, mockNotifications, mockLogger, mockModals, mockSystemStatus, eventBus } =
	vi.hoisted(() => {
		let subscriber: ((value: unknown) => void) | null = null;

		return {
			mockApi: {
				getDevices: vi.fn(),
				addDevice: vi.fn(),
				deleteDevice: vi.fn(),
				controlDevice: vi.fn()
			},
			mockNotifications: {
				success: vi.fn(),
				error: vi.fn()
			},
			mockLogger: {
				error: vi.fn()
			},
			mockModals: {
				open: vi.fn(),
				close: vi.fn()
			},
			mockSystemStatus: {
				subscribeChannel: vi.fn(),
				unsubscribeChannel: vi.fn(),
				getSnapshot: vi.fn<(channel?: string) => unknown | null>(() => null)
			},
			eventBus: {
				subscribe(run: (value: unknown) => void) {
					subscriber = run;
					run(null);
					return () => {
						subscriber = null;
					};
				},
				emit(value: unknown) {
					subscriber?.(value);
				}
			}
		};
	});

function createDeferred<T>() {
	let resolve!: (value: T | PromiseLike<T>) => void;
	let reject!: (reason?: unknown) => void;
	const promise = new Promise<T>((res, rej) => {
		resolve = res;
		reject = rej;
	});
	return { promise, resolve, reject };
}

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal();
	return {
		// @ts-expect-error test override
		...actual,
		onMount: (fn: () => void) => fn(),
		onDestroy: vi.fn()
	};
});

vi.mock('svelte-modals', () => ({
	modals: mockModals
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: {}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	shelly_default_name: () => 'My Shelly',
	shelly_error_limit: ({ count }: { count: number }) => `limit:${count}`,
	shelly_error_ip_required: () => 'ip required',
	toast_shelly_device_added: () => 'added',
	toast_shelly_device_load_failed: () => 'load failed',
	toast_shelly_device_add_failed: () => 'add failed',
	toast_shelly_device_updated: () => 'updated',
	toast_shelly_device_update_failed: () => 'update failed',
	toast_shelly_device_delete_failed: () => 'delete failed',
	toast_shelly_device_deleted: () => 'deleted',
	shelly_error_toggle: () => 'toggle failed',
	toast_message: ({ message }: { message: string }) => message,
	shelly_delete_title: () => 'Delete?',
	shelly_delete_msg: () => 'Sure?',
	action_cancel: () => 'Cancel',
	action_delete: () => 'Delete'
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn((options: Record<string, unknown>) => mockModals.open({}, options))
}));

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown';
	})
}));

vi.mock('$lib/services/api/integrations/ShellyApiService', () => ({
	ShellyApiService: class {
		constructor() {
			return mockApi;
		}
	}
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: mockLogger
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: mockSystemStatus,
	systemEvents: eventBus
}));

vi.mock('$lib/stores/user', () => ({
	user: {
		username: 'tester',
		admin: true,
		bearer_token: 'token',
		isValid: true,
		invalidate: vi.fn()
	}
}));

function createAccess(canRead = true, canManage = true, bearerToken = '') {
	return {
		canRead,
		canManage,
		apiOptions: { bearerToken }
	};
}

describe('useShellyDevices', () => {
	beforeEach(async () => {
		vi.clearAllMocks();
		mockSystemStatus.getSnapshot.mockReturnValue(null);
		mockApi.getDevices.mockResolvedValue([]);
		const { shellyStore } = await import('$lib/stores/shelly.svelte');
		shellyStore.reset();
	});

	it('resets add-device draft on close', () => {
		const cleanup = $effect.root(() => {
			const shelly = useShellyDevices({
				access: createAccess(),
				initialDevicesFn: () => []
			});

			shelly.openAddModal();
			shelly.updateNewDeviceName('Desk Lamp');
			shelly.updateNewDeviceIp('http://192.168.1.40/');
			shelly.updateNewDeviceRelay(3);
			shelly.updateNewDeviceGeneration(1);
			shelly.closeAddModal();

			expect(shelly.state.adding).toBe(false);
			expect(shelly.state.error).toBeNull();
			expect(shelly.state.newDevice).toEqual({
				id: '',
				name: 'My Shelly',
				ip: '',
				relay_index: 0,
				generation: 2
			});
		});

		cleanup();
	});

	it('guards against duplicate add requests while save is in flight', async () => {
		const deferred = createDeferred<void>();
		mockApi.addDevice.mockReturnValue(deferred.promise);

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const shelly = useShellyDevices({
					access: createAccess(),
					initialDevicesFn: () => []
				});

				shelly.openAddModal();
				shelly.updateNewDeviceIp('192.168.1.50');

				void shelly.saveDevice();
				void shelly.saveDevice();

				expect(mockApi.addDevice).toHaveBeenCalledTimes(1);
				expect(shelly.state.saving).toBe(true);

				deferred.resolve();
				setTimeout(() => {
					expect(shelly.state.saving).toBe(false);
					expect(shelly.state.adding).toBe(false);
					expect(mockNotifications.success).toHaveBeenCalledWith('added', 3000);
					resolve();
				}, 0);
			});
		});

		cleanup?.();
	});

	it('opens edit modal and updates device without hitting add limit', async () => {
		mockApi.addDevice.mockResolvedValue(undefined);

		const existingDevices = [
			{
				id: 'a1',
				name: 'Desk',
				ip: '192.168.1.10',
				relay_index: 0,
				generation: 2,
				isOn: true,
				isOnline: true
			},
			{ id: 'a2', name: 'Lamp', ip: '192.168.1.11', relay_index: 0, generation: 2 },
			{ id: 'a3', name: 'Pump', ip: '192.168.1.12', relay_index: 0, generation: 2 },
			{ id: 'a4', name: 'Fan', ip: '192.168.1.13', relay_index: 0, generation: 2 }
		];

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const shelly = useShellyDevices({
					access: createAccess(),
					initialDevicesFn: () => [...existingDevices]
				});

				shelly.openEditModal(existingDevices[0]);
				expect(shelly.state.editingDeviceId).toBe('a1');
				expect(shelly.state.newDevice).toEqual({
					id: 'a1',
					name: 'Desk',
					ip: '192.168.1.10',
					relay_index: 0,
					generation: 2
				});

				shelly.updateNewDeviceName('Desk Updated');
				shelly.updateNewDeviceIp('http://192.168.1.99/');
				void shelly.saveDevice();

				setTimeout(() => {
					expect(mockApi.addDevice).toHaveBeenCalledWith({
						id: 'a1',
						name: 'Desk Updated',
						ip: '192.168.1.99',
						relay_index: 0,
						generation: 2,
						enabled: true
					});
					expect(mockNotifications.success).toHaveBeenCalledWith('updated', 3000);
					expect(shelly.state.adding).toBe(false);
					expect(shelly.state.editingDeviceId).toBeNull();
					expect(shelly.state.error).toBeNull();
					expect(shelly.state.devices[0]).toMatchObject({
						id: 'a1',
						name: 'Desk Updated',
						ip: '192.168.1.99',
						isOn: true,
						isOnline: true
					});
					resolve();
				}, 0);
			});
		});

		cleanup?.();
	});

	it('blocks saves for non-IPv4 Shelly targets', async () => {
		const cleanup = $effect.root(() => {
			const shelly = useShellyDevices({
				access: createAccess(),
				initialDevicesFn: () => []
			});

			shelly.openAddModal();
			shelly.updateNewDeviceIp('http://shelly.local/relay/0');
			void shelly.saveDevice();

			expect(mockApi.addDevice).not.toHaveBeenCalled();
			expect(shelly.state.error).toBe('ip required');
			expect(shelly.state.newDevice.ip).toBe('shelly.local');
		});

		cleanup();
	});

	it('reverts optimistic toggles when relay control is rejected', async () => {
		mockApi.controlDevice.mockRejectedValue(new Error('Shelly device not found'));
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const shelly = useShellyDevices({
					access: createAccess(),
					initialDevicesFn: () => [
						{
							id: 'a1',
							name: 'Desk',
							ip: '192.168.1.10',
							relay_index: 0,
							generation: 2,
							isOn: false,
							isOnline: true
						}
					]
				});

				void shelly.toggleDevice('a1', true);
				expect(shelly.state.devices[0].isOn).toBe(true);

				setTimeout(() => {
					expect(mockApi.controlDevice).toHaveBeenCalledWith({ id: 'a1', on: true });
					expect(shelly.state.devices[0].isOn).toBe(false);
					expect(shelly.state.error).toBe('Shelly device not found');
					expect(mockNotifications.error).toHaveBeenCalledWith('Shelly device not found', 4000);
					resolve();
				}, 0);
			});
		});

		cleanup?.();
	});

	it('stays read-only when management is disabled', async () => {
		const cleanup = $effect.root(() => {
			const shelly = useShellyDevices({
				access: createAccess(true, false, 'user-token'),
				initialDevicesFn: () => []
			});

			shelly.openAddModal();
			void shelly.saveDevice();
			void shelly.toggleDevice('a1', true);
			void shelly.removeDevice('a1');

			expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('shelly');
			expect(shelly.state.adding).toBe(false);
			expect(shelly.state.editingDeviceId).toBeNull();
			expect(mockApi.addDevice).not.toHaveBeenCalled();
			expect(mockApi.controlDevice).not.toHaveBeenCalled();
		});

		cleanup();
	});

	it('falls back to GET for read-only users when no cached snapshot is available', async () => {
		mockApi.getDevices.mockResolvedValue([
			{
				id: 'guest-1',
				name: 'Guest Plug',
				ip: '192.168.1.60',
				relay_index: 0,
				generation: 2,
				isOn: false,
				isOnline: true
			}
		]);

		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const shelly = useShellyDevices({
					access: createAccess(true, false, 'user-token'),
					initialDevicesFn: () => []
				});

				void vi.waitFor(() => {
					expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('shelly');
					expect(mockApi.getDevices).toHaveBeenCalledTimes(1);
					expect(shelly.state.loading).toBe(false);
					expect(shelly.state.devices).toEqual([
						expect.objectContaining({
							id: 'guest-1',
							name: 'Guest Plug'
						})
					]);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('starts Shelly loading once read access becomes available after mount', async () => {
		mockApi.getDevices.mockResolvedValue([
			{
				id: 'late-1',
				name: 'Late Plug',
				ip: '192.168.1.61',
				relay_index: 0,
				generation: 2,
				isOn: false,
				isOnline: true
			}
		]);

		let canRead = $state(false);
		let canManage = $state(false);
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				const shelly = useShellyDevices({
					access: {
						get canRead() {
							return canRead;
						},
						get canManage() {
							return canManage;
						},
						get apiOptions() {
							return { bearerToken: 'late-token' };
						}
					},
					initialDevicesFn: () => []
				});

				expect(shelly.state.loading).toBe(false);
				expect(mockSystemStatus.subscribeChannel).not.toHaveBeenCalled();

				canRead = true;
				canManage = true;

				void vi.waitFor(() => {
					expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('shelly');
					expect(mockApi.getDevices).toHaveBeenCalledTimes(1);
					expect(shelly.state.loading).toBe(false);
					expect(shelly.state.devices).toEqual([
						expect.objectContaining({
							id: 'late-1',
							name: 'Late Plug'
						})
					]);
					expect(shelly.canManage).toBe(true);

					shelly.openAddModal();
					expect(shelly.state.adding).toBe(true);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('hydrates Shelly devices from a cached websocket snapshot before subscribing live', async () => {
		mockSystemStatus.getSnapshot.mockReturnValue([
			{
				id: 'cached-1',
				name: 'Cached Plug',
				ip: '192.168.1.44',
				relay_index: 0,
				generation: 2,
				isOn: true,
				isOnline: true
			}
		]);

		const cleanup = $effect.root(() => {
			const shelly = useShellyDevices({
				access: createAccess(),
				initialDevicesFn: () => []
			});

			expect(mockSystemStatus.getSnapshot).toHaveBeenCalledWith('shelly');
			expect(mockSystemStatus.subscribeChannel).toHaveBeenCalledWith('shelly');
			expect(shelly.state.loading).toBe(false);
			expect(shelly.state.devices).toEqual([
				expect.objectContaining({
					id: 'cached-1',
					name: 'Cached Plug'
				})
			]);
			expect(mockApi.getDevices).not.toHaveBeenCalled();
		});

		cleanup();
	});
});
