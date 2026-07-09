// @vitest-environment jsdom
import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import MatrixDataVisualization from './MatrixDataVisualization.svelte';
import type { MatrixSettings } from '$lib/services/api/core/MatrixApiService';

const { mockGetDataVisualizationStatus, mockGetBleStatus } = vi.hoisted(() => ({
	mockGetDataVisualizationStatus: vi.fn(),
	mockGetBleStatus: vi.fn()
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => ({
		canRead: true,
		apiOptions: {}
	})
}));

vi.mock('$lib/services/api/core/MatrixApiService', () => ({
	MatrixApiService: vi.fn().mockImplementation(function () {
		return {
			getDataVisualizationStatus: mockGetDataVisualizationStatus
		};
	})
}));

vi.mock('$lib/services/api/connectivity/BleApiService', () => ({
	BleApiService: vi.fn().mockImplementation(function () {
		return {
			getStatus: mockGetBleStatus
		};
	})
}));

vi.mock('$lib/paraglide/messages.js', () => {
	const labels: Record<string, string> = {
		action_save: 'Save',
		action_discard: 'Discard',
		common_loading: 'Loading',
		matrix_background_mode: 'Matrix background',
		matrix_background_mode_off: 'Off',
		matrix_background_mode_effects: 'Effects',
		matrix_background_mode_live_data: 'Live data',
		matrix_data_viz_title: 'Live data',
		matrix_data_viz_desc: 'Live matrix data',
		matrix_data_viz_source: 'Source',
		matrix_data_viz_source_scd4x: 'SCD4X',
		matrix_data_viz_source_ble: 'BLE',
		matrix_data_viz_source_wifi_rssi: 'Wi-Fi RSSI',
		matrix_data_viz_source_wifi_csi: 'Wi-Fi CSI',
		matrix_data_viz_metric: 'Metric',
		matrix_data_viz_metric_co2: 'CO2',
		matrix_data_viz_metric_temperature: 'Temperature',
		matrix_data_viz_metric_humidity: 'Humidity',
		matrix_data_viz_metric_rssi: 'RSSI',
		matrix_data_viz_metric_signal_quality: 'Signal quality',
		matrix_data_viz_metric_csi_motion: 'CSI motion',
		matrix_data_viz_mode: 'Visualization mode',
		matrix_data_viz_mode_gauge: 'Gauge',
		matrix_data_viz_mode_heatmap: 'Heatmap',
		matrix_data_viz_mode_trend: 'Trend',
		matrix_data_viz_mode_spectrum_bars: 'Spectrum bars',
		matrix_data_viz_mode_perimeter_meter: 'Perimeter',
		matrix_data_viz_mode_pulse: 'Pulse',
		matrix_data_viz_min: 'Minimum',
		matrix_data_viz_max: 'Maximum',
		matrix_data_viz_colors: 'Colors',
		matrix_data_viz_color_low: 'Low',
		matrix_data_viz_color_mid_label: 'Mid',
		matrix_data_viz_color_high: 'High',
		matrix_data_viz_color_min: 'Minimum color',
		matrix_data_viz_color_mid: 'Middle color',
		matrix_data_viz_color_max: 'Maximum color',
		matrix_data_viz_brightness_min: 'Minimum brightness',
		matrix_data_viz_brightness_max: 'Maximum brightness',
		matrix_data_viz_smoothing: 'Smoothing',
		matrix_data_viz_stale_behavior: 'When data is missing',
		matrix_data_viz_stale_dim: 'Dim',
		matrix_data_viz_stale_gray: 'Gray',
		matrix_data_viz_stale_blank: 'Blank',
		matrix_data_viz_background_effects_action: 'Open effects settings',
		matrix_data_viz_ble_device: 'BLE sensor',
		matrix_data_viz_ble_device_placeholder: 'Auto: first active sensor',
		matrix_data_viz_ble_load_error: 'BLE unavailable',
		matrix_data_viz_ble_last_seen_unknown: 'unknown',
		matrix_data_viz_csi_consumer_active: 'consumer active',
		matrix_data_viz_csi_consumer_inactive: 'consumer inactive',
		matrix_data_viz_csi_last_packet_never: 'never',
		matrix_data_viz_status_title: 'Source status',
		matrix_data_viz_status_live: 'Live',
		matrix_data_viz_status_no_fresh_data: 'No fresh data',
		matrix_data_viz_status_load_error: 'Status unavailable',
		matrix_data_viz_reason_ok: 'ok',
		matrix_data_viz_reason_disabled: 'disabled',
		matrix_data_viz_reason_no_service: 'no service',
		matrix_data_viz_reason_no_sample: 'no sample',
		matrix_data_viz_reason_stale: 'stale',
		matrix_data_viz_reason_wifi_inactive: 'Wi-Fi inactive',
		matrix_data_viz_reason_no_ble_device: 'no BLE device',
		matrix_data_viz_reason_no_scd4x_reading: 'no SCD4X reading',
		matrix_data_viz_reason_no_csi_packet: 'no CSI packet',
		tooltip_refresh: 'Refresh'
	};

	return {
		...Object.fromEntries(Object.entries(labels).map(([key, value]) => [key, () => value])),
		matrix_data_viz_ble_last_seen_age: ({ seconds }: { seconds: number }) => `${seconds}s ago`,
		matrix_data_viz_status_summary: ({
			value,
			age,
			bins,
			reason
		}: {
			value: string;
			age: string;
			bins: number;
			reason: string;
		}) => `${value} ${age} ${bins} ${reason}`,
		matrix_data_viz_csi_status_summary: ({
			consumer,
			packets,
			last
		}: {
			consumer: string;
			packets: number;
			last: string;
		}) => `CSI: ${consumer} ${packets} ${last}`
	};
});

function createMatrixSettings(overrides: Partial<MatrixSettings> = {}): MatrixSettings {
	return {
		brightness: 20,
		alarm_mode: 1,
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
		background_mode: 1,
		data_visualization_enabled: true,
		data_visualization_source: 2,
		data_visualization_metric: 4,
		data_visualization_mode: 3,
		data_visualization_min: 0,
		data_visualization_max: 100,
		data_visualization_color_min: 0x00ff80,
		data_visualization_color_mid: 0xffd166,
		data_visualization_color_max: 0xff3000,
		data_visualization_brightness_min: 12,
		data_visualization_brightness_max: 180,
		data_visualization_smoothing: 50,
		data_visualization_stale_behavior: 0,
		data_visualization_device_id: '',
		menu_enabled: true,
		menu_text_color: 0xffffff,
		menu_scroll_speed: 20,
		...overrides
	};
}

function createMatrixStore(overrides: Partial<MatrixSettings> = {}) {
	const settings = $state(createMatrixSettings(overrides));
	return {
		error: undefined,
		loading: false,
		saving: false,
		hasChanges: true,
		settings,
		loadSettings: vi.fn(),
		saveSettingsNow: vi.fn(async () => true),
		saveSettingsSilentlyNow: vi.fn(),
		saveSettings: vi.fn(),
		resetSettings: vi.fn(),
		updateSetting: vi.fn()
	};
}

describe('MatrixDataVisualization', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockGetDataVisualizationStatus.mockResolvedValue({
			active: true,
			source: 2,
			metric: 4,
			mode: 3,
			valid: true,
			stale: false,
			reason: 'ok',
			value: 80,
			secondary: 0,
			last_update_ms: 1000,
			age_ms: 20,
			bin_count: 0,
			csi: {
				available: true,
				matrix_visualization_consumer_active: false,
				packets_per_sec: 0,
				last_packet_ms: 0
			}
		});
		mockGetBleStatus.mockResolvedValue({
			enabled: true,
			running: true,
			scanner_active: true,
			devices: []
		});
	});

	afterEach(() => {
		cleanup();
	});

	it('filters visualization modes by selected source and normalizes to the default', async () => {
		const store = createMatrixStore();
		render(MatrixDataVisualization, { props: { store, canManage: true } });

		let modeSelect = screen.getByRole('combobox', {
			name: 'Visualization mode'
		}) as HTMLSelectElement;
		expect(Array.from(modeSelect.options).map((option) => option.text)).toEqual([
			'Gauge',
			'Trend',
			'Perimeter',
			'Pulse'
		]);

		await fireEvent.change(screen.getByRole('combobox', { name: 'Source' }), {
			target: { value: '3' }
		});

		modeSelect = screen.getByRole('combobox', { name: 'Visualization mode' }) as HTMLSelectElement;
		expect(Array.from(modeSelect.options).map((option) => option.text)).toEqual([
			'Heatmap',
			'Spectrum bars',
			'Pulse'
		]);
		expect(store.settings.data_visualization_source).toBe(3);
		expect(store.settings.data_visualization_metric).toBe(5);
		expect(store.settings.data_visualization_mode).toBe(2);
		expect(store.settings.data_visualization_stale_behavior).toBe(2);
	});

	it('writes canonical background booleans for Off, Effects, and Live data', async () => {
		const store = createMatrixStore();
		render(MatrixDataVisualization, { props: { store, canManage: true } });
		const backgroundSelect = screen.getByRole('combobox', {
			name: 'Matrix background'
		}) as HTMLSelectElement;

		await fireEvent.change(backgroundSelect, { target: { value: '2' } });
		expect(store.settings.background_mode).toBe(2);
		expect(store.settings.effect_enabled).toBe(false);
		expect(store.settings.data_visualization_enabled).toBe(false);

		await fireEvent.change(backgroundSelect, { target: { value: '0' } });
		expect(store.settings.background_mode).toBe(0);
		expect(store.settings.effect_enabled).toBe(true);
		expect(store.settings.data_visualization_enabled).toBe(false);

		await fireEvent.change(backgroundSelect, { target: { value: '1' } });
		expect(store.settings.background_mode).toBe(1);
		expect(store.settings.effect_enabled).toBe(false);
		expect(store.settings.data_visualization_enabled).toBe(true);
	});

	it('does not mark inactive but valid source snapshots as live', async () => {
		mockGetDataVisualizationStatus.mockResolvedValueOnce({
			active: false,
			source: 2,
			metric: 4,
			mode: 3,
			valid: true,
			stale: false,
			reason: 'disabled',
			value: 77,
			secondary: 0,
			last_update_ms: 1000,
			age_ms: 20,
			bin_count: 0,
			csi: {
				available: true,
				matrix_visualization_consumer_active: false,
				packets_per_sec: 0,
				last_packet_ms: 0
			}
		});

		const store = createMatrixStore();
		render(MatrixDataVisualization, { props: { store, canManage: true } });

		expect(await screen.findByText('No fresh data')).toBeTruthy();
		expect(screen.queryByText('Live')).toBeNull();
	});

	it('shows CSI diagnostics when Wi-Fi CSI is selected', async () => {
		mockGetDataVisualizationStatus.mockResolvedValueOnce({
			active: true,
			source: 3,
			metric: 5,
			mode: 2,
			valid: true,
			stale: false,
			reason: 'ok',
			value: 42,
			secondary: 0,
			last_update_ms: 1000,
			age_ms: 20,
			bin_count: 64,
			csi: {
				available: true,
				matrix_visualization_consumer_active: true,
				packets_per_sec: 7,
				last_packet_ms: 1234
			}
		});

		const store = createMatrixStore({
			data_visualization_source: 3,
			data_visualization_metric: 5,
			data_visualization_mode: 2,
			data_visualization_stale_behavior: 2
		});
		render(MatrixDataVisualization, { props: { store, canManage: true } });

		expect(await screen.findByText('CSI: consumer active 7 1234 ms')).toBeTruthy();
	});

	it('links to effects settings when the shared background is set to effects', () => {
		const store = createMatrixStore({
			background_mode: 0,
			effect_enabled: true,
			data_visualization_enabled: false
		});
		render(MatrixDataVisualization, { props: { store, canManage: true } });

		expect(screen.getByRole('link', { name: 'Open effects settings' }).getAttribute('href')).toBe(
			'/system/matrix/effects'
		);
	});
});
