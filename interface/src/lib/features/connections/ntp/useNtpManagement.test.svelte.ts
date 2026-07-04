import { beforeEach, describe, expect, it, vi } from 'vitest';
import type { NTPSettings, NTPStatus } from '$lib/types/connectivity/ntp';

const { mockNotifications } = vi.hoisted(() => ({
	mockNotifications: {
		success: vi.fn(),
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
	settings_load_error: () => 'load error',
	ntp_error_save: () => 'save ntp error',
	ntp_error_manual_time: () => 'manual time error',
	ntp_msg_save_success: () => 'ntp saved',
	ntp_msg_manual_success: () => 'manual time saved',
	toast_message: ({ message }: { message: string }) => message
}));

function createSettings(overrides: Partial<NTPSettings> = {}): NTPSettings {
	return {
		enabled: true,
		server: 'time.google.com',
		tz_label: 'Europe/London',
		tz_format: 'GMT0BST,M3.5.0/1,M10.5.0',
		...overrides
	};
}

function createStatus(overrides: Partial<NTPStatus> = {}): NTPStatus {
	return {
		status: 1,
		time_valid: true,
		utc_time: '2026-04-02T10:00:00Z',
		local_time: '2026-04-02T11:00:00+01:00',
		server: 'time.google.com',
		uptime: 120,
		...overrides
	};
}

describe('useNtpManagement', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('loads status and settings for admin sessions', async () => {
		const { useNtpManagement } = await import('./useNtpManagement.svelte');
		const api = {
			getStatus: vi.fn().mockResolvedValue(createStatus()),
			getSettings: vi.fn().mockResolvedValue(createSettings({ server: 'pool.ntp.org' })),
			saveSettings: vi.fn(),
			setTime: vi.fn()
		};

		let ntp!: ReturnType<typeof useNtpManagement>;
		const cleanup = $effect.root(() => {
			ntp = useNtpManagement({
				api: api as never,
				session: { canManage: true }
			});
		});

		await ntp.load();

		expect(api.getStatus).toHaveBeenCalledOnce();
		expect(api.getSettings).toHaveBeenCalledOnce();
		expect(ntp.status).toEqual(createStatus());
		expect(ntp.settingsLoaded).toBe(true);
		expect(ntp.settings.server).toBe('pool.ntp.org');

		cleanup();
	});

	it('auto-loads once per shouldLoad cycle', async () => {
		const { useNtpManagement } = await import('./useNtpManagement.svelte');
		const api = {
			getStatus: vi.fn().mockResolvedValue(createStatus()),
			getSettings: vi.fn().mockResolvedValue(createSettings()),
			saveSettings: vi.fn(),
			setTime: vi.fn()
		};

		let cleanup: (() => void) | undefined;
		let setCanLoad!: (value: boolean) => void;

		cleanup = $effect.root(() => {
			let canLoad = $state(false);
			setCanLoad = (value: boolean) => {
				canLoad = value;
			};

			useNtpManagement({
				api: api as never,
				session: { canManage: true },
				shouldLoad: () => canLoad
			});
		});

		expect(api.getStatus).not.toHaveBeenCalled();
		expect(api.getSettings).not.toHaveBeenCalled();

		setCanLoad(true);
		await vi.waitFor(() => {
			expect(api.getStatus).toHaveBeenCalledTimes(1);
			expect(api.getSettings).toHaveBeenCalledTimes(1);
		});

		await Promise.resolve();
		expect(api.getStatus).toHaveBeenCalledTimes(1);
		expect(api.getSettings).toHaveBeenCalledTimes(1);

		setCanLoad(false);
		await Promise.resolve();
		setCanLoad(true);

		await vi.waitFor(() => {
			expect(api.getStatus).toHaveBeenCalledTimes(2);
			expect(api.getSettings).toHaveBeenCalledTimes(2);
		});

		cleanup?.();
	});

	it('skips settings loading for read-only sessions', async () => {
		const { useNtpManagement } = await import('./useNtpManagement.svelte');
		const api = {
			getStatus: vi.fn().mockResolvedValue(createStatus()),
			getSettings: vi.fn(),
			saveSettings: vi.fn(),
			setTime: vi.fn()
		};

		let ntp!: ReturnType<typeof useNtpManagement>;
		const cleanup = $effect.root(() => {
			ntp = useNtpManagement({
				api: api as never,
				session: { canManage: false }
			});
		});

		await ntp.load();

		expect(api.getStatus).toHaveBeenCalledOnce();
		expect(api.getSettings).not.toHaveBeenCalled();
		expect(ntp.settingsLoaded).toBe(false);

		cleanup();
	});

	it('applies manual time without forcing session invalidation', async () => {
		const { useNtpManagement } = await import('./useNtpManagement.svelte');
		const api = {
			getStatus: vi.fn().mockResolvedValue(createStatus()),
			getSettings: vi.fn().mockResolvedValue(createSettings()),
			saveSettings: vi.fn().mockResolvedValue(createSettings({ enabled: false })),
			setTime: vi.fn().mockResolvedValue(undefined)
		};

		let ntp!: ReturnType<typeof useNtpManagement>;
		const cleanup = $effect.root(() => {
			ntp = useNtpManagement({
				api: api as never,
				session: { canManage: true },
				getBrowserTime: () => '2026-04-02T12:34:56'
			});
		});

		await ntp.load();
		ntp.settings = createSettings({ enabled: false });
		ntp.manualTimeInput = '2026-04-02T12:34:56';

		const saved = await ntp.saveSettings();

		expect(saved).toBe(true);
		expect(api.saveSettings).toHaveBeenCalledWith(expect.objectContaining({ enabled: false }));
		expect(api.setTime).toHaveBeenCalledWith('2026-04-02T12:34:56');
		expect(mockNotifications.success).toHaveBeenNthCalledWith(1, 'ntp saved', 3000);
		expect(mockNotifications.success).toHaveBeenNthCalledWith(2, 'manual time saved', 3000);

		cleanup();
	});
});
