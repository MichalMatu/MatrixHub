import { describe, expect, it, vi } from 'vitest';
import type { UdpSettings } from '$lib/services/api/integrations/UdpApiService';
import { useUdpSettings } from './useUdpSettings.svelte';

function createSettings(overrides: Partial<UdpSettings> = {}): UdpSettings {
	return {
		enabled: false,
		host: '',
		port: 8089,
		format: 'line',
		interval_ms: 60000,
		...overrides
	};
}

function flushPromises() {
	return new Promise<void>((resolve) => queueMicrotask(() => queueMicrotask(resolve)));
}

describe('useUdpSettings', () => {
	it('normalizes loaded settings from the API', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				host: 'demo.local\u0001',
				port: 0,
				format: 'broken',
				interval_ms: 5000
			}),
			updateSettings: vi.fn()
		};

		let udpState!: ReturnType<typeof useUdpSettings>;
		const cleanup = $effect.root(() => {
			udpState = useUdpSettings({
				api: api as never,
				feedback: {}
			});
		});

		await udpState.loadSettings();

		expect(udpState.settings).toEqual(
			createSettings({
				enabled: true,
				host: 'demo.local',
				port: 8089,
				format: 'line',
				interval_ms: 60000
			})
		);
		expect(udpState.canTest).toBe(true);

		cleanup();
	}, 10000);

	it('sanitizes host edits and blocks save when enabled host is blank', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue(createSettings({ enabled: true, host: 'demo.local' })),
			updateSettings: vi.fn()
		};

		let udpState!: ReturnType<typeof useUdpSettings>;
		const cleanup = $effect.root(() => {
			udpState = useUdpSettings({
				api: api as never,
				feedback: {}
			});
		});

		await udpState.loadSettings();
		udpState.updateSetting('host', ' \u0001 ');
		expect(udpState.settings.host).toBe('');
		expect(udpState.canTest).toBe(true);

		udpState.saveSettings();
		await flushPromises();

		expect(api.updateSettings).not.toHaveBeenCalled();
		expect(udpState.errors.host).toBe(true);

		cleanup();
	}, 10000);

	it('keeps canTest true until a disabling save succeeds', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue(createSettings({ enabled: true, host: 'demo.local' })),
			updateSettings: vi
				.fn()
				.mockResolvedValue(createSettings({ enabled: false, host: '', port: 8089 }))
		};

		let udpState!: ReturnType<typeof useUdpSettings>;
		const cleanup = $effect.root(() => {
			udpState = useUdpSettings({
				api: api as never,
				feedback: {}
			});
		});

		await udpState.loadSettings();
		expect(udpState.canTest).toBe(true);

		udpState.updateSetting('enabled', false);
		expect(udpState.canTest).toBe(true);

		udpState.saveSettings();
		await flushPromises();

		expect(udpState.canTest).toBe(false);

		cleanup();
	}, 10000);

	it('saves a sanitized host payload', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue(createSettings()),
			updateSettings: vi.fn().mockResolvedValue(
				createSettings({
					enabled: true,
					host: 'example.local',
					port: 9000,
					format: 'json'
				})
			)
		};

		let udpState!: ReturnType<typeof useUdpSettings>;
		const cleanup = $effect.root(() => {
			udpState = useUdpSettings({
				api: api as never,
				feedback: {}
			});
		});

		await udpState.loadSettings();
		udpState.updateSetting('enabled', true);
		udpState.updateSetting('host', ' example.local\u0001 ');
		udpState.updateSetting('port', 9000);
		udpState.setFormat('json');

		udpState.saveSettings();
		await flushPromises();

		expect(api.updateSettings).toHaveBeenCalledWith(
			createSettings({
				enabled: true,
				host: 'example.local',
				port: 9000,
				format: 'json'
			})
		);

		cleanup();
	}, 10000);
});
