import { beforeEach, describe, expect, it, vi } from 'vitest';

const { mockPage, mockUser, refs, modalStack, mockNotifications, mockCreateService } = vi.hoisted(
	() => ({
		mockPage: {
			data: {
				features: {
					ntp: true,
					sleep: true
				},
				title: '',
				titleOverride: false
			},
			url: new URL('http://localhost/charts')
		},
		mockUser: {
			admin: true,
			bearer_token: 'token'
		},
		refs: {
			systemStatus: null as unknown,
			connectionState: null as unknown
		},
		modalStack: {
			open: vi.fn(),
			close: vi.fn(),
			closeById: vi.fn()
		},
		mockNotifications: {
			error: vi.fn()
		},
		mockCreateService: vi.fn()
	})
);

class MockSystemStatusStore {
	data = $state({
		timestamp: 0,
		lastUpdate: 0,
		wifiStatus: 0,
		rssi: -60,
		isConnected: true,
		isStaConnected: true,
		isApMode: false,
		coreTemp: 44
	});

	setData(next: Partial<typeof this.data>) {
		Object.assign(this.data, next);
	}
}

class MockConnectionState {
	status = $state<'connected' | 'connecting' | 'disconnected' | 'error'>('connected');
	reconnectAttempt = $state(0);
	lastError = $state<string | null>(null);

	setState(
		status: 'connected' | 'connecting' | 'disconnected' | 'error',
		options: { reconnectAttempt?: number; lastError?: string | null } = {}
	) {
		this.status = status;
		this.reconnectAttempt = options.reconnectAttempt ?? 0;
		this.lastError = options.lastError ?? null;
	}
}

const mockSystemStatusStore = new MockSystemStatusStore();
const mockConnectionState = new MockConnectionState();
refs.systemStatus = mockSystemStatusStore;
refs.connectionState = mockConnectionState;

vi.mock('svelte', async (importOriginal) => {
	const actual = await importOriginal<typeof import('svelte')>();
	return {
		...actual,
		onMount: (fn: () => void) => fn(),
		onDestroy: vi.fn()
	};
});

vi.mock('$app/state', () => ({
	page: mockPage
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

vi.mock('$lib/stores/systemStatus.svelte', () => ({
	systemStatus: refs.systemStatus
}));

vi.mock('$lib/stores/connectionState.svelte', () => ({
	connectionState: refs.connectionState
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		createService: mockCreateService
	})
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: {}
}));

vi.mock('$lib/components/toasts/notifications.svelte', () => ({
	notifications: mockNotifications
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn((options: { modalService?: typeof modalStack } & Record<string, unknown>) => {
		const { modalService = modalStack, component: _component, ...props } = options;
		return modalService.open({}, props);
	})
}));

vi.mock('$lib/utils/ui/modal', () => ({
	defaultModalStack: modalStack,
	openModal: vi.fn(),
	closeModalById: vi.fn()
}));

vi.mock('$lib/utils', () => ({
	toUserRequestErrorMessage: vi.fn(() => 'sleep failed')
}));

vi.mock('./featureRegistry', () => ({
	resolveFeatureTitle: vi.fn(() => 'Charts')
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	app_title: () => 'MatrixHub',
	sleep_confirm_title: () => 'Sleep?',
	sleep_confirm_msg: () => 'Enter sleep?',
	sleep_abort_btn: () => 'Cancel',
	sleep_enter_btn: () => 'Sleep',
	sleep_request_failed: () => 'Sleep failed.',
	statusbar_api_connected: () => 'API and live status connected',
	statusbar_api_connecting: ({ attempt }: { attempt: number }) =>
		`Reconnecting live status, attempt ${attempt}`,
	statusbar_api_disconnected: () => 'Live status disconnected',
	statusbar_api_error: ({ error }: { error: string }) => `Live status issue: ${error}`,
	request_error_network: () => 'Network error. Device may be unreachable.',
	error_prefix: ({ error }: { error: string }) => `Error: ${error}`
}));

describe('useStatusbarManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockPage.data.title = '';
		mockPage.data.titleOverride = false;
		mockPage.url = new URL('http://localhost/charts');
		mockSystemStatusStore.setData({
			isConnected: true,
			isStaConnected: true
		});
		mockConnectionState.setState('connected');
		mockCreateService.mockReturnValue({ requestSleep: vi.fn().mockResolvedValue(undefined) });
	});

	it('exposes live connection state for the statusbar indicator', async () => {
		const { useStatusbarManagement } = await import('./useStatusbarManagement.svelte');
		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const statusbar = useStatusbarManagement({
				modalStack,
				now: () => 0
			});

			expect(statusbar.connectionStatus).toBe('connected');
			expect(statusbar.connectionClass).toBe('text-success');
			expect(statusbar.connectionTooltip).toBe('API and live status connected');

			mockConnectionState.setState('connecting', { reconnectAttempt: 3 });
			expect(statusbar.connectionStatus).toBe('connecting');
			expect(statusbar.connectionClass).toBe('text-warning');
			expect(statusbar.connectionTooltip).toBe('Reconnecting live status, attempt 3');

			mockConnectionState.setState('error', { lastError: 'System WebSocket Disconnected' });
			expect(statusbar.connectionStatus).toBe('error');
			expect(statusbar.connectionClass).toBe('text-error');
			expect(statusbar.connectionTooltip).toBe('Live status issue: System WebSocket Disconnected');

			mockConnectionState.setState('disconnected');
			expect(statusbar.connectionStatus).toBe('disconnected');
			expect(statusbar.connectionTooltip).toBe('Live status disconnected');
		});

		cleanup?.();
	});

	it('shows error toast when sleep request fails', async () => {
		const requestSleep = vi.fn().mockRejectedValue(new Error('offline'));
		mockCreateService.mockReturnValue({ requestSleep });
		const { useStatusbarManagement } = await import('./useStatusbarManagement.svelte');
		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const statusbar = useStatusbarManagement({
				modalStack,
				now: () => 0
			});

			statusbar.confirmSleep();
			const openCall = modalStack.open.mock.calls.at(-1);
			expect(openCall).toBeTruthy();
			const props = openCall?.[1] as { onConfirm: () => void };

			void props.onConfirm();
		});

		await vi.waitFor(() => {
			expect(requestSleep).toHaveBeenCalledOnce();
			expect(mockNotifications.error).toHaveBeenCalledWith('Error: sleep failed', 4000);
		});

		cleanup?.();
	});
});
