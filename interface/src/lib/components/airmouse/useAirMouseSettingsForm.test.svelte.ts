import { describe, expect, it, vi } from 'vitest';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { useAirMouseSettingsForm } from './useAirMouseSettingsForm.svelte';

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
	restart_confirm_msg_generic: () => 'restart generic'
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
			mode: 0,
			interval: 60,
			move_distance: 1,
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

describe('useAirMouseSettingsForm', () => {
	it('marks movement and click toggles as restart-required', () => {
		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			const form = useAirMouseSettingsForm(() => ({
				status: createStatus(),
				saveSettings: vi.fn(async () => true)
			}));

			form.settings.movement_enabled = false;

			expect(form.requiresRestart).toBe(true);
		});

		cleanup?.();
	});

	it('uses restart confirmation when usb-affecting toggles change', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let status = $state(createStatus());

				const saveSettings = vi.fn().mockImplementation(async () => {
					status = createStatus({ movement_enabled: false });
					return true;
				});

				const form = useAirMouseSettingsForm(() => ({
					get status() {
						return status;
					},
					saveSettings
				}));

				form.settings.movement_enabled = false;
				form.confirmSave();

				expect(mockConfirmRestartAndSave).toHaveBeenCalledOnce();
				const [onSave, options] = mockConfirmRestartAndSave.mock.calls[0];
				expect(options.message).toBe('restart generic');

				void onSave().then(() => {
					expect(saveSettings).toHaveBeenCalledWith(
						expect.objectContaining({
							movement_enabled: false,
							click_enabled: true
						})
					);
					resolve();
				});
			});
		});

		cleanup?.();
	});
	it('does not mark omitted script fields as dirty after sync', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let status = $state(
					createStatus({
						single_click_script: undefined as unknown as string,
						double_click_script: undefined as unknown as string,
						triple_click_script: undefined as unknown as string
					})
				);

				const form = useAirMouseSettingsForm(() => ({
					get status() {
						return status;
					},
					saveSettings: vi.fn(async () => true)
				}));

				queueMicrotask(() => {
					expect(form.settings.single_click_script).toBe('');
					expect(form.settings.double_click_script).toBe('');
					expect(form.settings.triple_click_script).toBe('');
					expect(form.hasChanges).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('does not overwrite unsaved edits on background status refresh', () => {
		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			let status = $state(createStatus());

			const form = useAirMouseSettingsForm(() => ({
				get status() {
					return status;
				},
				saveSettings: vi.fn(async () => true)
			}));

			form.settings.sensitivityX = 450;
			status = createStatus({ sensitivity_x: 250 });

			expect(form.settings.sensitivityX).toBe(450);
			expect(form.hasChanges).toBe(true);
		});

		cleanup?.();
	});

	it('re-syncs to canonical backend values after a successful save', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let status = $state(createStatus({ sensitivity_x: 200, acceleration_factor: 3 }));

				const saveSettings = vi.fn().mockImplementation(async () => {
					status = createStatus({
						sensitivity_x: 300,
						acceleration_enabled: false,
						acceleration_factor: 1,
						single_click_script: undefined as unknown as string
					});
					return true;
				});

				const form = useAirMouseSettingsForm(() => ({
					get status() {
						return status;
					},
					saveSettings
				}));

				form.settings.sensitivityX = 280;
				form.settings.accelerationFactor = 1;

				void form.save().then(() => {
					expect(saveSettings).toHaveBeenCalledWith(
						expect.objectContaining({
							sensitivity_x: 280,
							acceleration_enabled: false,
							acceleration_factor: 1
						})
					);
					expect(form.settings.sensitivityX).toBe(300);
					expect(form.settings.accelerationFactor).toBe(1);
					expect(form.settings.single_click_script).toBe('');
					expect(form.hasChanges).toBe(false);
					resolve();
				});
			});
		});

		cleanup?.();
	});

	it('keeps local edits visible while save is in flight and syncs after backend refresh', async () => {
		let cleanup: (() => void) | undefined;

		await new Promise<void>((resolve) => {
			cleanup = $effect.root(() => {
				let status = $state(createStatus({ sensitivity_x: 200 }));
				let releaseSave: (() => void) | null = null;

				const saveSettings = vi.fn().mockImplementation(
					() =>
						new Promise<boolean>((resolveSave) => {
							releaseSave = () => {
								status = createStatus({ sensitivity_x: 300 });
								resolveSave(true);
							};
						})
				);

				const form = useAirMouseSettingsForm(() => ({
					get status() {
						return status;
					},
					saveSettings
				}));

				queueMicrotask(() => {
					form.settings.sensitivityX = 280;
					const savePromise = form.save();

					queueMicrotask(() => {
						expect(form.settings.sensitivityX).toBe(280);
						releaseSave?.();

						void savePromise.then(() => {
							expect(form.settings.sensitivityX).toBe(300);
							expect(form.hasChanges).toBe(false);
							resolve();
						});
					});
				});
			});
		});

		cleanup?.();
	});
});
