import { beforeEach, describe, expect, it } from 'vitest';
import type { AlarmRule } from '$lib/types/domain/alarms';

type AlarmFormState = ReturnType<typeof import('./useAlarmRuleForm.svelte').useAlarmRuleForm>;

function createRule(overrides: Partial<AlarmRule> = {}): AlarmRule {
	return {
		id: 'alarm-1',
		enabled: true,
		name: 'High temp',
		source: 'temperature',
		operator: 'above',
		threshold: 30,
		severity: 'warning',
		notify_channels: ['telegram'],
		cooldown_seconds: 600,
		shelly_device_ids: [],
		ble_device_mac: '',
		created_at: 100,
		updated_at: 100,
		...overrides
	};
}

function flushPromises() {
	return new Promise((resolve) => setTimeout(resolve, 0));
}

describe('useAlarmRuleForm', () => {
	beforeEach(() => {
		// no-op; keep structure aligned with other hook tests
	});

	it('initializes a new rule when modal opens and generates auto name', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		expect(form.formData.enabled).toBe(true);
		expect(form.formData.notify_channels).toEqual(['led']);
		expect(form.formData.name).toBe('Temp > 30°C (LED)');
		expect(form.formValid).toBe(true);
		expect(form.isEditing).toBe(false);

		cleanup();
	});

	it('resets threshold on source change and preserves manual name edits', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		form.formData.name = 'Custom alarm';
		form.handleSourceChange('co2');

		expect(form.formData.source).toBe('co2');
		expect(form.formData.threshold).toBe(1000);
		expect(form.formData.name).toBe('Custom alarm');

		cleanup();
	});

	it('builds saved rule with injected id and timestamp when draft is valid', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen,
				{
					now: () => 999,
					createId: () => 'generated-id'
				}
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		form.formData.source = 'ble_temperature';
		form.formData.ble_device_mac = 'aa:bb';
		form.handleThresholdChange(24);
		form.handleNotifyChannelsChange(['telegram', 'webhook']);

		const saved = form.submitRule();

		expect(saved).toMatchObject({
			id: 'generated-id',
			source: 'ble_temperature',
			threshold: 24,
			ble_device_mac: 'aa:bb',
			notify_channels: ['telegram', 'webhook'],
			created_at: 999,
			updated_at: 999
		});

		cleanup();
	});

	it('returns null for invalid BLE draft without selected device', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
			edit: (rule: AlarmRule) => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen
			);

			control = {
				open: () => {
					isOpen = true;
				},
				edit: (rule: AlarmRule) => {
					currentRule = rule;
					isOpen = true;
				}
			};
		});

		control.edit(
			createRule({
				source: 'ble_temperature',
				name: 'BLE temp',
				ble_device_mac: ''
			})
		);
		await flushPromises();

		expect(form.isBleSource).toBe(true);
		expect(form.formValid).toBe(false);
		expect(form.submitRule()).toBeNull();

		cleanup();
	});

	it('treats BLE battery and RSSI as BLE-backed sources', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		form.handleSourceChange('ble_battery');
		expect(form.isBleSource).toBe(true);
		expect(form.thresholdConfig).toEqual({ min: 0, max: 100, step: 1, default: 20 });
		expect(form.formData.threshold).toBe(20);
		expect(form.formData.name).toBe('BLE Batt < 20% (LED)');

		form.handleSourceChange('ble_rssi');
		expect(form.isBleSource).toBe(true);
		expect(form.thresholdConfig).toEqual({ min: -120, max: -20, step: 1, default: -90 });
		expect(form.formData.threshold).toBe(-90);
		expect(form.formData.name).toBe('BLE RSSI < -90dBm (LED)');

		cleanup();
	});

	it('treats CSI motion as boolean-like above 0.5', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		form.handleSourceChange('wifi_csi_motion');
		expect(form.isBooleanLikeSource).toBe(true);
		expect(form.isBleSource).toBe(false);
		expect(form.thresholdConfig).toEqual({ min: 0, max: 1, step: 0.5, default: 0.5 });
		expect(form.formData.operator).toBe('above');
		expect(form.formData.threshold).toBe(0.5);
		expect(form.formData.name).toBe('CSI motion detected (LED)');

		form.handleOperatorChange('below');
		form.handleThresholdChange(0);
		expect(form.formData.operator).toBe('above');
		expect(form.formData.threshold).toBe(0.5);

		cleanup();
	});

	it('treats IMU tamper as boolean-like above 0.5 with its own auto name', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		form.handleSourceChange('imu_tamper');
		expect(form.isBooleanLikeSource).toBe(true);
		expect(form.isBleSource).toBe(false);
		expect(form.thresholdConfig).toEqual({ min: 0, max: 1, step: 0.5, default: 0.5 });
		expect(form.formData.operator).toBe('above');
		expect(form.formData.threshold).toBe(0.5);
		expect(form.formData.name).toBe('IMU tamper detected (LED)');

		form.handleOperatorChange('below');
		form.handleThresholdChange(0);
		expect(form.formData.operator).toBe('above');
		expect(form.formData.threshold).toBe(0.5);

		cleanup();
	});

	it('requires a GPIO channel for GPIO digital alarms', async () => {
		const { useAlarmRuleForm } = await import('./useAlarmRuleForm.svelte');

		let form!: AlarmFormState;
		let control!: {
			open: () => void;
		};

		const cleanup = $effect.root(() => {
			let isOpen = $state(false);
			let currentRule = $state<AlarmRule | null>(null);

			form = useAlarmRuleForm(
				() => currentRule,
				() => isOpen,
				{
					now: () => 999,
					createId: () => 'generated-id'
				}
			);

			control = {
				open: () => {
					isOpen = true;
				}
			};
		});

		control.open();
		await flushPromises();

		form.handleSourceChange('gpio_digital');
		expect(form.isGpioSource).toBe(true);
		expect(form.isBooleanLikeSource).toBe(true);
		expect(form.thresholdConfig).toEqual({ min: 0, max: 1, step: 0.5, default: 0.5 });
		expect(form.formData.operator).toBe('above');
		expect(form.formData.threshold).toBe(0.5);
		expect(form.formData.name).toBe('GPIO input true (LED)');
		expect(form.formValid).toBe(false);
		expect(form.submitRule()).toBeNull();

		form.formData.gpio_id = 'gpio38';
		expect(form.formValid).toBe(true);
		expect(form.submitRule()).toMatchObject({
			id: 'generated-id',
			source: 'gpio_digital',
			gpio_id: 'gpio38',
			threshold: 0.5,
			created_at: 999,
			updated_at: 999
		});

		cleanup();
	});
});
