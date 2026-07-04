// @vitest-environment jsdom
import { fireEvent, render, screen, within } from '@testing-library/svelte';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import MatrixAlarmSettings from './MatrixAlarmSettings.svelte';
import MatrixEffects from './MatrixEffects.svelte';
import MatrixSettings from './MatrixSettings.svelte';
import type { MatrixSettings as MatrixSettingsModel } from '$lib/services/api/core/MatrixApiService';

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => {
	const effectMessages = Object.fromEntries(
		[
			'matrix_eff_static',
			'matrix_eff_blink',
			'matrix_eff_breath',
			'matrix_eff_color_wipe',
			'matrix_eff_color_wipe_inv',
			'matrix_eff_color_wipe_rev',
			'matrix_eff_color_wipe_rev_inv',
			'matrix_eff_color_wipe_random',
			'matrix_eff_random_color',
			'matrix_eff_single_dynamic',
			'matrix_eff_multi_dynamic',
			'matrix_eff_rainbow',
			'matrix_eff_rainbow_cycle',
			'matrix_eff_scan',
			'matrix_eff_dual_scan',
			'matrix_eff_fade',
			'matrix_eff_theater_chase',
			'matrix_eff_theater_chase_rainbow',
			'matrix_eff_running_lights',
			'matrix_eff_twinkle',
			'matrix_eff_twinkle_random',
			'matrix_eff_twinkle_fade',
			'matrix_eff_twinkle_fade_random',
			'matrix_eff_sparkle',
			'matrix_eff_flash_sparkle',
			'matrix_eff_hyper_sparkle',
			'matrix_eff_strobe',
			'matrix_eff_strobe_rainbow',
			'matrix_eff_multi_strobe',
			'matrix_eff_blink_rainbow',
			'matrix_eff_chase_white',
			'matrix_eff_chase_color',
			'matrix_eff_chase_random',
			'matrix_eff_chase_rainbow',
			'matrix_eff_chase_flash',
			'matrix_eff_chase_flash_random',
			'matrix_eff_chase_rainbow_white',
			'matrix_eff_chase_blackout',
			'matrix_eff_chase_blackout_rainbow',
			'matrix_eff_color_sweep_random',
			'matrix_eff_running_color',
			'matrix_eff_running_red_blue',
			'matrix_eff_running_random',
			'matrix_eff_larson_scanner',
			'matrix_eff_comet',
			'matrix_eff_fireworks',
			'matrix_eff_fireworks_random',
			'matrix_eff_merry_christmas',
			'matrix_eff_fire_flicker',
			'matrix_eff_fire_flicker_soft',
			'matrix_eff_fire_flicker_intense',
			'matrix_eff_circus_combustus',
			'matrix_eff_halloween',
			'matrix_eff_bicolor_chase',
			'matrix_eff_tricolor_chase',
			'matrix_eff_twinklefox',
			'matrix_eff_rain',
			'matrix_eff_block_dissolve',
			'matrix_eff_icu',
			'matrix_eff_dual_larson',
			'matrix_eff_running_random2',
			'matrix_eff_filler_up',
			'matrix_eff_rainbow_larson',
			'matrix_eff_rainbow_fireworks',
			'matrix_eff_trifade',
			'matrix_eff_heartbeat',
			'matrix_eff_bits',
			'matrix_eff_multi_comet',
			'matrix_eff_popcorn',
			'matrix_eff_oscillator'
		].map((key) => [key, () => key])
	);

	return {
		...effectMessages,
		action_cancel: () => 'Cancel',
		action_close: () => 'Close',
		action_clear: () => 'Clear',
		action_default: () => 'Default',
		action_discard: () => 'Discard',
		action_fill: () => 'Fill',
		action_save: () => 'Save',
		common_loading: () => 'Loading',
		settings_title: () => 'Settings',
		matrix_title: () => 'Matrix LED Settings',
		matrix_display_title: () => 'Matrix Display',
		matrix_alarm_title: () => 'Matrix Alarms',
		matrix_section_alarm: () => 'Alarm',
		matrix_section_menu: () => 'Menu',
		matrix_section_display: () => 'Display',
		matrix_mode_label: () => 'Alarm Display Mode',
		matrix_mode_solid: () => 'Solid Color',
		matrix_mode_solid_desc: () => 'Fill screen',
		matrix_mode_icon: () => 'Icon',
		matrix_mode_icon_desc: () => 'Show icons',
		matrix_mode_scroll: () => 'Scroll Text',
		matrix_mode_scroll_desc: () => 'Scroll alarm',
		matrix_custom_icons_title: () => 'Custom Icons',
		matrix_custom_icons_desc: () => 'Customize icons',
		matrix_edit_icons_btn: () => 'Edit Icons',
		matrix_icon_tab_info: () => 'Info',
		matrix_icon_tab_warning: () => 'Warning',
		matrix_icon_tab_critical: () => 'Critical',
		matrix_menu_enabled: () => 'Button Menu',
		matrix_menu_enabled_desc: () => 'Physical matrix menu',
		matrix_menu_text_color: () => 'Menu Text Color',
		matrix_menu_text_color_desc: () => 'Text color',
		matrix_scroll_speed: () => 'Scroll Speed',
		matrix_brightness: () => 'Brightness',
		matrix_rotation: () => 'Screen Rotation',
		matrix_auto: () => 'Auto',
		matrix_auto_desc: () => 'Use IMU orientation',
		matrix_icon_editor_title: () => 'Icon Editor',
		matrix_icon_editor_pixel: ({ index }: { index: string }) => `Pixel ${index}`,
		matrix_icon_editor_select_color: ({ color }: { color: string }) => `Select ${color}`,
		matrix_icon_editor_custom_color: () => 'Custom color',
		matrix_effects_title: () => 'Visual Effects',
		matrix_effects_enable: () => 'Enable Effects',
		matrix_effects_desc: () => 'Runs when idle',
		matrix_effects_disabled_hint: () => 'Effect setup is kept',
		matrix_effect_category: () => 'Category',
		matrix_effect_category_recommended: () => 'Recommended',
		matrix_effect_category_calm: () => 'Calm',
		matrix_effect_category_dynamic: () => 'Dynamic',
		matrix_effect_category_seasonal: () => 'Seasonal',
		matrix_effect_category_all: () => 'All',
		matrix_effect_engine: () => 'Effect Engine',
		matrix_effect_engine_classic: () => 'Classic',
		matrix_effect_engine_native_3d: () => 'Reactive 3D',
		matrix_effect_mode: () => 'Effect Mode',
		matrix_effect_mode_live_desc: () => 'Saved with the card Save action',
		matrix_effect_speed: () => 'Animation Speed',
		matrix_effect_reactivity_provider: () => 'Reactivity Provider',
		matrix_effect_reactivity_provider_none: () => 'Off',
		matrix_effect_reactivity_provider_imu: () => 'IMU Motion',
		matrix_effect_reactivity_gain: () => 'Reactivity Gain',
		matrix_effect_palettes: () => 'Palettes',
		matrix_effect_palettes_desc: () => 'Preset colors',
		matrix_effect_palette_alert: () => 'Alert',
		matrix_effect_palette_forest: () => 'Forest',
		matrix_effect_palette_ocean: () => 'Ocean',
		matrix_effect_palette_sunset: () => 'Sunset',
		matrix_effect_palette_neon: () => 'Neon',
		matrix_effect_palette_aurora: () => 'Aurora',
		matrix_base_color: () => 'Base Color',
		matrix_color_desc: () => 'Used by effects',
		matrix_effect_color_primary: () => 'Primary color',
		matrix_effect_color_secondary: () => 'Secondary color',
		matrix_effect_color_tertiary: () => 'Tertiary color',
		matrix_eff_3d_gyro_cube: () => 'Iridescent Ripple',
		matrix_eff_3d_gravity_particles: () => 'Gravity Particles',
		matrix_eff_3d_depth_tunnel: () => 'Depth Tunnel',
		matrix_eff_3d_liquid_wave: () => 'Liquid Wave',
		matrix_eff_3d_oil_sheen: () => 'Oil Sheen',
		matrix_eff_3d_aurora_curtain: () => 'Aurora Curtain',
		matrix_eff_3d_plasma_cloud: () => 'Plasma Cloud',
		matrix_eff_3d_orbital_rings: () => 'Orbital Rings',
		matrix_eff_3d_tilt_vortex: () => 'Tilt Vortex',
		matrix_eff_3d_comet_field: () => 'Comet Field',
		matrix_eff_3d_lava_lamp: () => 'Lava Lamp',
		matrix_eff_3d_prism_sweep: () => 'Prism Sweep',
		matrix_eff_3d_rain_glass: () => 'Rain Glass',
		matrix_eff_3d_breathing_terrain: () => 'Breathing Terrain',
		matrix_eff_3d_palette_wave: () => 'Palette Wave',
		imu_state_enabled: () => 'Enabled',
		imu_state_disabled: () => 'Disabled'
	};
});

function createMatrixSettings(overrides: Partial<MatrixSettingsModel> = {}): MatrixSettingsModel {
	return {
		brightness: 20,
		alarm_mode: 1,
		rotation: 0,
		auto_rotate: false,
		effect_enabled: true,
		effect_engine: 0,
		effect_mode: 2,
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
		menu_enabled: true,
		menu_text_color: 0xffffff,
		menu_scroll_speed: 20,
		...overrides
	};
}

function createMatrixStore(
	overrides: Partial<MatrixSettingsModel> = {},
	stateOverrides: { hasChanges?: boolean } = {}
) {
	const settings = $state(createMatrixSettings(overrides));
	const updateSetting = vi.fn(
		<K extends keyof MatrixSettingsModel>(key: K, value: MatrixSettingsModel[K]) => {
			settings[key] = value;
		}
	);

	return {
		error: undefined,
		loading: false,
		saving: false,
		hasChanges: stateOverrides.hasChanges ?? true,
		settings,
		loadSettings: vi.fn(),
		saveSettingsNow: vi.fn(async () => true),
		saveSettingsSilentlyNow: vi.fn(),
		saveSettings: vi.fn(),
		resetSettings: vi.fn(),
		updateSetting
	};
}

describe('Matrix cards', () => {
	beforeEach(() => {
		vi.clearAllMocks();
	});

	it('keeps effect selection as a draft instead of silently saving it', async () => {
		const store = createMatrixStore();
		render(MatrixEffects, { props: { store, canManage: true } });

		await fireEvent.change(screen.getByRole('combobox', { name: 'Effect Mode' }), {
			target: { value: '11' }
		});

		expect(store.updateSetting).toHaveBeenCalledWith('effect_mode', 11);
		expect(store.settings.effect_mode).toBe(11);
		expect(store.saveSettingsSilentlyNow).not.toHaveBeenCalled();
		expect(store.saveSettings).not.toHaveBeenCalled();
	});

	it('stages the native 3D engine and IMU reactivity as effect drafts', async () => {
		const store = createMatrixStore({ effect_mode: 69 });
		render(MatrixEffects, { props: { store, canManage: true } });

		await fireEvent.change(screen.getByRole('combobox', { name: 'Effect Engine' }), {
			target: { value: '1' }
		});
		await fireEvent.change(screen.getByRole('combobox', { name: 'Reactivity Provider' }), {
			target: { value: '1' }
		});
		await fireEvent.input(screen.getByLabelText('Reactivity Gain'), {
			target: { value: '125' }
		});

		expect(store.settings.effect_engine).toBe(1);
		expect(store.settings.effect_mode).toBe(14);
		expect(store.settings.effect_reactivity_provider).toBe(1);
		expect(store.settings.effect_reactivity_gain).toBe(125);
		expect((screen.getByRole('combobox', { name: 'Effect Mode' }) as HTMLSelectElement).value).toBe(
			'14'
		);
		expect(store.saveSettingsSilentlyNow).not.toHaveBeenCalled();
	});

	it('uses real disabled controls while visual effects are turned off', () => {
		const store = createMatrixStore({ effect_enabled: false });
		render(MatrixEffects, { props: { store, canManage: true } });

		const effectMode = screen.getByRole('combobox', { name: 'Effect Mode' }) as HTMLSelectElement;
		const primaryColor = screen.getByLabelText('Primary color') as HTMLInputElement;
		const secondsSpeedButton = screen.getByRole('button', { name: 's' }) as HTMLButtonElement;

		expect(effectMode.disabled).toBe(true);
		expect(primaryColor.disabled).toBe(true);
		expect(secondsSpeedButton.disabled).toBe(true);
	});

	it('exposes a local Save action for matrix display settings', async () => {
		const store = createMatrixStore();
		render(MatrixSettings, { props: { store, canManage: true } });

		await fireEvent.click(screen.getByRole('button', { name: 'Save' }));

		expect(store.saveSettingsNow).toHaveBeenCalledOnce();
		expect(store.saveSettings).not.toHaveBeenCalled();
	});

	it('disables the matrix card Save action when there are no draft changes', () => {
		const store = createMatrixStore({}, { hasChanges: false });
		render(MatrixSettings, { props: { store, canManage: true } });

		const saveButton = screen.getByRole('button', { name: 'Save' }) as HTMLButtonElement;
		const discardButton = screen.getByRole('button', { name: 'Discard' }) as HTMLButtonElement;

		expect(saveButton.disabled).toBe(true);
		expect(discardButton.disabled).toBe(true);
	});

	it('resets matrix drafts through the local discard action', async () => {
		const store = createMatrixStore();
		render(MatrixSettings, { props: { store, canManage: true } });

		await fireEvent.click(screen.getByRole('button', { name: 'Discard' }));

		expect(store.resetSettings).toHaveBeenCalledOnce();
	});

	it('guards matrix saves against double clicks', async () => {
		let resolveSave: ((value: boolean) => void) | undefined;
		const store = createMatrixStore();
		store.saveSettingsNow.mockImplementation(
			() =>
				new Promise<boolean>((resolve) => {
					resolveSave = resolve;
				})
		);
		render(MatrixSettings, { props: { store, canManage: true } });

		const saveButton = screen.getByRole('button', { name: 'Save' });
		void fireEvent.click(saveButton);
		await fireEvent.click(saveButton);

		expect(store.saveSettingsNow).toHaveBeenCalledOnce();
		resolveSave?.(true);
	});

	it('stages custom icons as a draft instead of saving from the modal', async () => {
		const store = createMatrixStore();
		render(MatrixAlarmSettings, { props: { store, canManage: true } });

		await fireEvent.click(screen.getByRole('button', { name: 'Edit Icons' }));
		await fireEvent.click(within(screen.getByRole('dialog')).getByRole('button', { name: 'Save' }));

		expect(store.settings.custom_icons).toEqual([[], [], []]);
		expect(store.saveSettingsNow).not.toHaveBeenCalled();
	});
});
