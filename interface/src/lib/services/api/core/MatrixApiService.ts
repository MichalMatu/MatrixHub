import { createApiClient, type ApiClientOptions } from '$lib/utils';
import { MatrixSettingsSchema } from '$lib/utils/validation/schemas';

export interface MatrixSettings {
	brightness: number;
	alarm_mode: number;
	rotation: number;
	auto_rotate: boolean;
	// Effects
	effect_enabled: boolean;
	effect_engine: number;
	effect_mode: number;
	effect_speed: number;
	effect_color: number;
	effect_color_2: number;
	effect_color_3: number;
	effect_reactivity_provider: number;
	effect_reactivity_gain: number;
	background_mode: number;
	data_visualization_enabled: boolean;
	data_visualization_source: number;
	data_visualization_metric: number;
	data_visualization_mode: number;
	data_visualization_min: number;
	data_visualization_max: number;
	data_visualization_color_min: number;
	data_visualization_color_mid: number;
	data_visualization_color_max: number;
	data_visualization_brightness_min: number;
	data_visualization_brightness_max: number;
	data_visualization_smoothing: number;
	data_visualization_stale_behavior: number;
	data_visualization_device_id: string;
	custom_icons?: number[][];
	// Menu settings
	menu_enabled: boolean;
	menu_text_color: number;
	menu_scroll_speed: number;
}

export class MatrixApiService {
	private client;
	private static readonly SETTINGS_TIMEOUT_MS = 20000;
	private static readonly SAVE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getSettings(): Promise<MatrixSettings> {
		return this.client.get<MatrixSettings>('/api/matrix/settings', {
			signal: AbortSignal.timeout(MatrixApiService.SETTINGS_TIMEOUT_MS),
			schema: MatrixSettingsSchema
		});
	}

	async updateSettings(settings: Partial<MatrixSettings>): Promise<MatrixSettings> {
		return this.client.post<MatrixSettings>('/api/matrix/settings', settings, {
			signal: AbortSignal.timeout(MatrixApiService.SAVE_TIMEOUT_MS),
			schema: MatrixSettingsSchema
		});
	}

	async calibrateCsiDataVisualization(): Promise<{ ok: boolean; status?: string }> {
		return this.client.post<{ ok: boolean; status?: string }>(
			'/api/matrix/data-visualization/csi/calibrate',
			{},
			{
				signal: AbortSignal.timeout(MatrixApiService.SAVE_TIMEOUT_MS)
			}
		);
	}
}
