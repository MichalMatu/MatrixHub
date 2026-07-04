import { beforeEach, describe, expect, it, vi } from 'vitest';

import { MatrixSettingsSchema } from '$lib/utils/validation/schemas';
import { MatrixApiService, type MatrixSettings } from './MatrixApiService';

const mockClient = {
	get: vi.fn(),
	post: vi.fn()
};

vi.mock('$lib/utils', () => ({
	createApiClient: () => mockClient
}));

function createMatrixSettings(overrides: Partial<MatrixSettings> = {}): MatrixSettings {
	return {
		brightness: 10,
		alarm_mode: 2,
		rotation: 0,
		auto_rotate: false,
		effect_enabled: false,
		effect_engine: 0,
		effect_mode: 0,
		effect_speed: 1000,
		effect_color: 0x00ff00,
		effect_color_2: 0xff0000,
		effect_color_3: 0x0000ff,
		effect_reactivity_provider: 0,
		effect_reactivity_gain: 80,
		background_mode: 0,
		data_visualization_enabled: false,
		data_visualization_source: 0,
		data_visualization_metric: 0,
		data_visualization_mode: 0,
		data_visualization_min: 400,
		data_visualization_max: 2000,
		data_visualization_color_min: 0x00ff80,
		data_visualization_color_mid: 0xffd166,
		data_visualization_color_max: 0xff3000,
		data_visualization_brightness_min: 12,
		data_visualization_brightness_max: 180,
		data_visualization_smoothing: 50,
		data_visualization_stale_behavior: 0,
		data_visualization_device_id: '',
		custom_icons: [[], [], []],
		menu_enabled: true,
		menu_text_color: 0xffffff,
		menu_scroll_speed: 60,
		...overrides
	};
}

describe('MatrixApiService', () => {
	let service: MatrixApiService;

	beforeEach(() => {
		vi.clearAllMocks();
		service = new MatrixApiService({ bearerToken: 'token' });
	});

	it('loads matrix settings', async () => {
		const settings = createMatrixSettings();
		mockClient.get.mockResolvedValue(settings);

		const result = await service.getSettings();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/matrix/settings',
			expect.objectContaining({ schema: MatrixSettingsSchema })
		);
		expect(result).toBe(settings);
	});

	it('saves matrix settings through the canonical POST endpoint', async () => {
		const payload = createMatrixSettings({ brightness: 42 });
		mockClient.post.mockResolvedValue(payload);

		const result = await service.updateSettings(payload);

		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/matrix/settings',
			payload,
			expect.objectContaining({ schema: MatrixSettingsSchema })
		);
		expect(result).toBe(payload);
	});

	it('keeps the legacy CSI data visualization calibration endpoint harmless', async () => {
		mockClient.post.mockResolvedValue({ ok: true, status: 'baseline_not_required' });

		const result = await service.calibrateCsiDataVisualization();

		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/matrix/data-visualization/csi/calibrate',
			{},
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
		expect(result).toEqual({ ok: true, status: 'baseline_not_required' });
	});
});
