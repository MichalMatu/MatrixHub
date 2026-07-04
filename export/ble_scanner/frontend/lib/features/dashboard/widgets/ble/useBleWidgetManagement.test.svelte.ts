import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import type { BleSettings } from '$lib/types/connectivity/ble';
import { useBleWidgetManagement } from './useBleWidgetManagement.svelte';

const { mockSession } = vi.hoisted(() => ({
	mockSession: {
		isAuthenticated: true
	}
}));

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => mockSession
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	time_ago_s: ({ s }: { s: number }) => `${s}s`,
	time_ago_m: ({ m, s }: { m: number; s: number }) => `${m}m ${s}s`,
	time_ago_h: ({ h, m }: { h: number; m: number }) => `${h}h ${m}m`
}));

function createBleStore(
	devices: Record<
		string,
		{
			mac: string;
			temp: number;
			humid: number;
			batt: number;
			rssi: number;
			lastSeen: number;
			alias?: string;
			saved?: boolean;
		}
	> = {},
	settings: BleSettings | null = null
) {
	let currentDevices = devices;
	let currentSettings = $state(settings);
	let currentSettingsLoading = $state(settings === null);

	return {
		start: vi.fn(),
		stop: vi.fn(),
		get devices() {
			return currentDevices;
		},
		get settings() {
			return currentSettings;
		},
		set settings(v: BleSettings | null) {
			currentSettings = v;
		},
		get settingsLoading() {
			return currentSettingsLoading;
		},
		set settingsLoading(v: boolean) {
			currentSettingsLoading = v;
		},
		settingsError: null as string | null,
		replaceDevices: vi.fn((nextDevices: Array<(typeof currentDevices)[string]>) => {
			currentDevices = Object.fromEntries(nextDevices.map((device) => [device.mac, device]));
		})
	};
}

describe('useBleWidgetManagement', () => {
	beforeEach(() => {
		vi.useFakeTimers();
	});

	afterEach(() => {
		vi.useRealTimers();
		vi.restoreAllMocks();
	});

	it('reads settings and devices from bleStore', async () => {
		const bleStore = createBleStore(
			{
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 48,
					batt: 90,
					rssi: -62,
					lastSeen: 1000
				}
			},
			{
				enabled: true,
				sensors: [{ mac: 'AA:BB:CC:DD:EE:FF', alias: 'Desk' }]
			}
		);

		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const widget = useBleWidgetManagement({ bleStore });

			expect(widget.settingsLoading).toBe(false);
			expect(widget.hasSensors).toBe(true);
			expect(widget.sensors[0]?.config.alias).toBe('Desk');
			expect(widget.sensors[0]?.data?.temp).toBe(21.5);
		});

		await vi.waitFor(() => {
			expect(bleStore.start).toHaveBeenCalledOnce();
		});

		cleanup?.();

		await vi.waitFor(() => {
			expect(bleStore.stop).toHaveBeenCalledOnce();
		});
	});

	it('shows no sensors when bleStore has no settings', async () => {
		const bleStore = createBleStore();

		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const widget = useBleWidgetManagement({ bleStore });

			expect(widget.settingsLoading).toBe(true);
			expect(widget.hasSensors).toBe(false);
			expect(widget.sensors).toHaveLength(0);
		});

		await vi.waitFor(() => {
			expect(bleStore.start).toHaveBeenCalledOnce();
		});

		cleanup?.();

		await vi.waitFor(() => {
			expect(bleStore.stop).toHaveBeenCalledOnce();
		});
	});

	it('reflects settings updates from bleStore', async () => {
		const bleStore = createBleStore();

		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const widget = useBleWidgetManagement({ bleStore });

			expect(widget.hasSensors).toBe(false);

			// Simulate bleStore receiving a snapshot
			bleStore.settings = {
				enabled: true,
				sensors: [{ mac: 'BB:BB:BB:BB:BB:BB', alias: 'Fresh' }]
			};
			bleStore.settingsLoading = false;

			expect(widget.settingsLoading).toBe(false);
			expect(widget.scannerEnabled).toBe(true);
		});

		await vi.waitFor(() => {
			expect(bleStore.start).toHaveBeenCalledOnce();
		});

		cleanup?.();

		await vi.waitFor(() => {
			expect(bleStore.stop).toHaveBeenCalledOnce();
		});
	});

	it('returns placeholder time when snapshot lastSeen is unavailable', () => {
		const bleStore = createBleStore();

		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const widget = useBleWidgetManagement({ bleStore });

			expect(widget.getTimeSince(0)).toBe('--');
		});

		cleanup?.();
	});

	it('clamps future timestamps to just-now style output', () => {
		const bleStore = createBleStore();

		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const widget = useBleWidgetManagement({ bleStore });

			expect(widget.getTimeSince(Date.now() + 10_000)).toBe('0s');
		});

		cleanup?.();
	});
});
