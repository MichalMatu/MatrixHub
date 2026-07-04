import { beforeEach, describe, expect, it, vi } from 'vitest';

const {
	mockApi,
	mockCreateService,
	mockNotifications,
	mockSubscribeChannel,
	mockUnsubscribeChannel,
	mockGetSnapshot,
	mockRequestSnapshot,
	mockEventSubscribe,
	mockEventUnsubscribe,
	loggerError,
	setEventCallback
} = vi.hoisted(() => {
	let eventCallback: ((value: unknown) => void) | null = null;

	return {
		mockApi: {
			triggerWifiRecovery: vi.fn()
		},
		mockCreateService: vi.fn(),
		mockNotifications: {
			error: vi.fn(),
			warning: vi.fn(),
			info: vi.fn(),
			success: vi.fn()
		},
		mockSubscribeChannel: vi.fn(),
		mockUnsubscribeChannel: vi.fn(),
		mockGetSnapshot: vi.fn(),
		mockRequestSnapshot: vi.fn(),
		mockEventUnsubscribe: vi.fn(),
		mockEventSubscribe: vi.fn((callback: (value: unknown) => void) => {
			eventCallback = callback;
			return mockEventUnsubscribe;
		}),
		loggerError: vi.fn(),
		setEventCallback: (value: unknown) => eventCallback?.(value)
	};
});

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal<typeof import('svelte')>();
	return {
		...actual,
		onMount: (fn: () => void) => fn(),
		onDestroy: vi.fn()
	};
});

vi.mock('$lib/services/api/core/SystemApiService', () => ({
	SystemApiService: class {}
}));

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: loggerError
	}
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: {
		subscribeChannel: mockSubscribeChannel,
		unsubscribeChannel: mockUnsubscribeChannel,
		getSnapshot: mockGetSnapshot,
		requestSnapshot: mockRequestSnapshot,
		subscribeEvents: mockEventSubscribe
	},
	systemEvents: {
		subscribe: mockEventSubscribe
	}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils', () => ({
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown error';
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	status_error_timeout: () => 'status timeout',
	status_wifi_recovery_timeout: () => 'recovery timeout',
	status_wifi_recovery_failed: () => 'recovery failed',
	status_wifi_recovery_connected: () => 'already connected',
	status_wifi_recovery_queued: () => 'recovery queued',
	status_wifi_recovery_rejected: () => 'recovery rejected',
	toast_message: ({ message }: { message: string }) => message
}));

type SystemStatusView = ReturnType<
	(typeof import('./useSystemStatusManagement.svelte'))['useSystemStatusManagement']
>;

describe('useSystemStatusManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.useRealTimers();
		mockCreateService.mockReturnValue(mockApi);
		mockApi.triggerWifiRecovery.mockResolvedValue({
			success: true,
			accepted: true,
			connected: false
		});
		mockGetSnapshot.mockReturnValue(null);
	});

	it('hydrates immediately from cached snapshot and relies on subscribe for revalidation', async () => {
		mockGetSnapshot.mockReturnValue({
			system_info: {
				uptime: 1234
			},
			diagnostics: {
				healthy: true,
				wifi: {
					rssi: -55,
					lastDisconnectReason: 0,
					healthy: true
				}
			},
			wifi_ap_mode: true
		} as never);
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		const cleanup = $effect.root(() => {
			const statusView = useSystemStatusManagement();

			expect(statusView.systemInfo?.uptime).toBe(1234);
			expect(statusView.loading).toBe(false);
			expect(statusView.refreshing).toBe(true);
			expect(statusView.isApMode).toBe(true);
			expect(mockSubscribeChannel).toHaveBeenCalledWith('system_status');
			expect(mockRequestSnapshot).not.toHaveBeenCalled();
		});

		cleanup();
	});

	it('falls back to timeout error when no snapshot arrives', async () => {
		vi.useFakeTimers();
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;
		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement({ snapshotTimeoutMs: 1000 });

			expect(statusView.loading).toBe(true);
		});

		await vi.advanceTimersByTimeAsync(1000);

		await vi.waitFor(() => {
			expect(statusView!.error).toBe('status timeout');
			expect(statusView!.loading).toBe(false);
			expect(statusView!.refreshing).toBe(false);
		});

		cleanup();
	});

	it('recovers cleanly when a delayed snapshot arrives after the timeout', async () => {
		vi.useFakeTimers();
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement({ snapshotTimeoutMs: 1000 });
		});

		await vi.advanceTimersByTimeAsync(1000);

		await vi.waitFor(() => {
			expect(statusView!.error).toBe('status timeout');
			expect(statusView!.loading).toBe(false);
			expect(statusView!.refreshing).toBe(false);
		});

		setEventCallback({
			type: 'snapshot',
			channel: 'system_status',
			data: {
				system_info: {
					uptime: 321
				},
				diagnostics: {
					healthy: true,
					wifi: {
						rssi: -55,
						lastDisconnectReason: 0,
						healthy: true
					}
				},
				wifi_ap_mode: false
			}
		} as never);

		expect(statusView!.systemInfo?.uptime).toBe(321);
		expect(statusView!.error).toBeNull();
		expect(statusView!.loading).toBe(false);
		expect(statusView!.refreshing).toBe(false);

		cleanup();
	});

	it('shows notification when WiFi recovery fails', async () => {
		mockApi.triggerWifiRecovery.mockRejectedValue(new Error('recover failed'));
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		await statusView!.triggerWifiRecovery();

		expect(loggerError).toHaveBeenCalled();
		expect(mockNotifications.error).toHaveBeenCalledWith('recover failed', 5000);

		cleanup();
	});

	it('shows success feedback when WiFi recovery is queued', async () => {
		mockApi.triggerWifiRecovery.mockResolvedValue({
			success: true,
			accepted: true,
			connected: false
		});
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		await statusView!.triggerWifiRecovery();

		expect(mockNotifications.success).toHaveBeenCalledWith('recovery queued', 4000);
		expect(mockRequestSnapshot).toHaveBeenCalledWith('system_status');

		cleanup();
	});

	it('shows info feedback when WiFi is already connected', async () => {
		mockApi.triggerWifiRecovery.mockResolvedValue({
			success: true,
			accepted: true,
			connected: true,
			ip: '192.168.0.30'
		});
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		await statusView!.triggerWifiRecovery();

		expect(mockNotifications.info).toHaveBeenCalledWith('already connected', 4000);

		cleanup();
	});

	it('shows warning feedback when WiFi recovery request is rejected', async () => {
		mockApi.triggerWifiRecovery.mockResolvedValue({
			success: false,
			accepted: false,
			connected: false
		});
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		await statusView!.triggerWifiRecovery();

		expect(mockNotifications.warning).toHaveBeenCalledWith('recovery rejected', 5000);

		cleanup();
	});

	it('applies incoming snapshot events after initial load', async () => {
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		setEventCallback({
			type: 'snapshot',
			channel: 'system_status',
			data: {
				system_info: {
					uptime: 999
				},
				diagnostics: {
					healthy: false,
					wifi: {
						rssi: -88,
						uptimeMs: 99,
						lastDisconnectReason: 1,
						healthy: false
					}
				},
				wifi_ap_mode: false
			}
		} as never);

		expect(statusView!.systemInfo?.uptime).toBe(999);
		expect(statusView!.health?.healthy).toBe(false);
		expect(statusView!.isApMode).toBe(false);
		expect(statusView!.loading).toBe(false);

		cleanup();
	});

	it('keeps AP mode visible when diagnostics report an active AP', async () => {
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		setEventCallback({
			type: 'snapshot',
			channel: 'system_status',
			data: {
				system_info: {
					uptime: 321
				},
				diagnostics: {
					healthy: true,
					wifi: {
						rssi: -70,
						lastDisconnectReason: 2,
						healthy: true,
						apActive: true
					},
					ap: {
						active: true,
						ip: '192.168.4.1'
					}
				},
				wifi_ap_mode: false
			}
		} as never);

		expect(statusView!.isApMode).toBe(true);
		expect(statusView!.health?.ap?.active).toBe(true);

		cleanup();
	});

	it('requests a fresh snapshot only after the initial subscribe bootstrap', async () => {
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		expect(mockSubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockRequestSnapshot).not.toHaveBeenCalled();
		statusView!.fetchAll();

		expect(mockSubscribeChannel).toHaveBeenCalledTimes(1);
		expect(mockRequestSnapshot).toHaveBeenCalledTimes(1);
		expect(mockRequestSnapshot).toHaveBeenCalledWith('system_status');

		cleanup();
	});

	it('stops refreshing and surfaces an error after the live runtime resets', async () => {
		const { useSystemStatusManagement } = await import('./useSystemStatusManagement.svelte');
		let statusView: SystemStatusView | null = null;

		const cleanup = $effect.root(() => {
			statusView = useSystemStatusManagement();
		});

		setEventCallback({
			type: 'snapshot',
			channel: 'system_status',
			data: {
				system_info: { uptime: 999 },
				diagnostics: {
					healthy: true,
					wifi: {
						rssi: -42,
						uptimeMs: 50,
						lastDisconnectReason: 0,
						healthy: true
					}
				},
				wifi_ap_mode: false
			}
		} as never);

		expect(statusView!.systemInfo?.uptime).toBe(999);

		mockGetSnapshot.mockReturnValue(null);
		setEventCallback(null);

		expect(statusView!.systemInfo).toBeNull();
		expect(statusView!.health).toBeNull();
		expect(statusView!.loading).toBe(false);
		expect(statusView!.refreshing).toBe(false);
		expect(statusView!.error).toBe('status timeout');

		cleanup();
	});
});
