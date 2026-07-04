import { describe, expect, it, vi } from 'vitest';
import { useCompensationSettings } from './useCompensationSettings.svelte';

function flushPromises() {
	return new Promise<void>((resolve) => queueMicrotask(() => queueMicrotask(resolve)));
}

describe('useCompensationSettings', () => {
	it('normalizes loaded settings and exposes enabled preview state', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				base_temp_offset: 999,
				reference_cpu_temp: 15,
				temp_offset_per_cpu_degree: -1,
				min_temp_offset: 30,
				max_temp_offset: -1
			}),
			updateSettings: vi.fn()
		};

		let compState!: ReturnType<typeof useCompensationSettings>;
		const cleanup = $effect.root(() => {
			compState = useCompensationSettings({
				api: api as never,
				feedback: {}
			});
		});

		await compState.loadSettings();

		expect(compState.settings).toEqual({
			enabled: true,
			base_temp_offset: 20,
			reference_cpu_temp: 20,
			temp_offset_per_cpu_degree: 0,
			min_temp_offset: 25,
			max_temp_offset: 25
		});
		expect(compState.canPreview).toBe(true);

		cleanup();
	}, 10000);

	it('applies presets without saving immediately', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				base_temp_offset: 2,
				reference_cpu_temp: 45,
				temp_offset_per_cpu_degree: 0.2,
				min_temp_offset: 0,
				max_temp_offset: 8
			}),
			updateSettings: vi.fn()
		};

		let compState!: ReturnType<typeof useCompensationSettings>;
		const cleanup = $effect.root(() => {
			compState = useCompensationSettings({
				api: api as never,
				feedback: {}
			});
		});

		await compState.loadSettings();
		compState.applyPreset('large');
		expect(compState.settings.base_temp_offset).toBe(4);
		expect(compState.settings.temp_offset_per_cpu_degree).toBe(0.3);
		expect(api.updateSettings).not.toHaveBeenCalled();

		cleanup();
	}, 10000);

	it('blocks invalid save when min offset exceeds max offset', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				base_temp_offset: 2,
				reference_cpu_temp: 45,
				temp_offset_per_cpu_degree: 0.2,
				min_temp_offset: 0,
				max_temp_offset: 8
			}),
			updateSettings: vi.fn()
		};

		let compState!: ReturnType<typeof useCompensationSettings>;
		const cleanup = $effect.root(() => {
			compState = useCompensationSettings({
				api: api as never,
				feedback: {}
			});
		});

		await compState.loadSettings();
		compState.updateSetting('min_temp_offset', 9);
		compState.updateSetting('max_temp_offset', 8);
		compState.saveSettings();
		await flushPromises();

		expect(api.updateSettings).not.toHaveBeenCalled();
		expect(compState.errors.min_temp_offset).toBe(true);
		expect(compState.errors.max_temp_offset).toBe(true);

		cleanup();
	}, 10000);

	it('keeps preview visible until disabling save succeeds', async () => {
		const api = {
			getSettings: vi.fn().mockResolvedValue({
				enabled: true,
				base_temp_offset: 2,
				reference_cpu_temp: 45,
				temp_offset_per_cpu_degree: 0.2,
				min_temp_offset: 0,
				max_temp_offset: 8
			}),
			updateSettings: vi.fn().mockResolvedValue({
				enabled: false,
				base_temp_offset: 2,
				reference_cpu_temp: 45,
				temp_offset_per_cpu_degree: 0.2,
				min_temp_offset: 0,
				max_temp_offset: 8
			})
		};

		let compState!: ReturnType<typeof useCompensationSettings>;
		const cleanup = $effect.root(() => {
			compState = useCompensationSettings({
				api: api as never,
				feedback: {}
			});
		});

		await compState.loadSettings();
		expect(compState.canPreview).toBe(true);

		compState.updateSetting('enabled', false);
		expect(compState.canPreview).toBe(true);

		compState.saveSettings();
		await flushPromises();

		expect(compState.canPreview).toBe(false);

		cleanup();
	}, 10000);
});
