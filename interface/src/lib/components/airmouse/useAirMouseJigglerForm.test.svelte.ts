import { describe, expect, it, vi } from 'vitest';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { useAirMouseJigglerForm } from './useAirMouseJigglerForm.svelte';
import { AIR_MOUSE_JIGGLER_MODES } from './airMouseConfig';

const { mockConfirmRestartAndSave } = vi.hoisted(() => ({
	mockConfirmRestartAndSave: vi.fn()
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/utils/ui/restartConfirmation', () => ({
	confirmRestartAndSave: mockConfirmRestartAndSave
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	restart_confirm_msg_generic: () => 'restart generic',
	jiggler_msg_saved: () => 'saved',
	settings_save_error: () => 'save error'
}));

function createStatus(overrides: Partial<AirMouseStatus> = {}): AirMouseStatus {
	return {
		movement_enabled: true,
		click_enabled: true,
		running: true,
		calibrating: false,
		sensitivity_x: 200,
		sensitivity_y: 180,
		deadzone: 2,
		acceleration_enabled: true,
		acceleration_factor: 3,
		tap_threshold_g: 0.5,
		click_debounce_ms: 200,
		double_click_window_ms: 500,
		click_source: 0,
		single_click_action: 1,
		double_click_action: 2,
		triple_click_action: 0,
		single_click_script: '',
		double_click_script: '',
		triple_click_script: '',
		euro_min_cutoff: 1,
		euro_beta: 0.1,
		euro_d_cutoff: 1,
		gyro_offset_x: 0,
		gyro_offset_y: 0,
		gyro_offset_z: 0,
		last_delta_g: 0,
		jiggler: {
			mode: 2,
			interval: 60,
			move_distance: 3,
			random_interval: false
		},
		imu: {
			gx: 0,
			gy: 0,
			gz: 0,
			ax: 0,
			ay: 0,
			az: 0
		},
		...overrides
	};
}

describe('useAirMouseJigglerForm', () => {
	it('marks jiggler enable-state changes as restart-required', () => {
		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const form = useAirMouseJigglerForm(() => ({
				status: createStatus({
					jiggler: {
						mode: AIR_MOUSE_JIGGLER_MODES.OFF,
						interval: 60,
						move_distance: 3,
						random_interval: false
					}
				}),
				saveSettings: vi.fn().mockResolvedValue(true),
				fetchStatus: vi.fn()
			}));

			form.settings.enabled = true;

			expect(form.requiresRestart).toBe(true);
		});

		cleanup?.();
	});

	it('uses restart confirmation when jiggler crosses the off/on boundary', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let status = $state(
					createStatus({
						jiggler: {
							mode: AIR_MOUSE_JIGGLER_MODES.OFF,
							interval: 60,
							move_distance: 3,
							random_interval: false
						}
					})
				);

				const saveSettings = vi.fn().mockResolvedValue(true);
				const fetchStatus = vi.fn().mockImplementation(async () => {
					status = createStatus({
						jiggler: {
							mode: AIR_MOUSE_JIGGLER_MODES.STEALTH,
							interval: 60,
							move_distance: 3,
							random_interval: false
						}
					});
				});

				const notifications = {
					success: vi.fn(),
					error: vi.fn()
				};

				const form = useAirMouseJigglerForm(
					() => ({
						get status() {
							return status;
						},
						saveSettings,
						fetchStatus
					}),
					{ notifications }
				);

				form.settings.enabled = true;
				form.confirmSave();

				expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
				const [onSave, options] = mockConfirmRestartAndSave.mock.calls[0];
				expect(options.message).toBe('restart generic');

				void onSave().then(() => {
					expect(saveSettings).toHaveBeenCalledWith(
						{
							jiggler: {
								mode: AIR_MOUSE_JIGGLER_MODES.STEALTH,
								interval: 60,
								move_distance: 3,
								random_interval: false
							}
						},
						false
					);
					resolve();
				});
			});
		});

		cleanup?.();
	});
	it('refreshes canonical jiggler settings after save instead of mutating stale status locally', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let status = $state(createStatus());

				const saveSettings = vi.fn().mockResolvedValue(true);
				const fetchStatus = vi.fn().mockImplementation(async () => {
					status = createStatus({
						jiggler: {
							mode: 4,
							interval: 120,
							move_distance: 7,
							random_interval: true
						}
					});
				});

				const notifications = {
					success: vi.fn(),
					error: vi.fn()
				};

				const form = useAirMouseJigglerForm(
					() => ({
						get status() {
							return status;
						},
						saveSettings,
						fetchStatus
					}),
					{ notifications }
				);

				form.settings.enabled = true;
				form.settings.mode = 1;
				form.settings.interval = 90;
				form.settings.distance = 5;
				form.settings.random = false;

				void form.save().then(() => {
					expect(saveSettings).toHaveBeenCalledWith(
						{
							jiggler: {
								mode: 1,
								interval: 90,
								move_distance: 5,
								random_interval: false
							}
						},
						false
					);
					expect(fetchStatus).toHaveBeenCalledOnce();
					expect(form.settings.mode).toBe(4);
					expect(form.settings.interval).toBe(120);
					expect(form.settings.distance).toBe(7);
					expect(form.settings.random).toBe(true);
					expect(form.hasChanges).toBe(false);
					expect(notifications.success).toHaveBeenCalledWith('saved', 3000);
					resolve();
				});
			});
		});

		cleanup?.();
	});
});
