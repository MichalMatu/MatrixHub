// @vitest-environment jsdom
import { cleanup, render, screen, within } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import ImuPage from './ImuPage.svelte';

const { mockSession, mockImuState, mockConsumerSettings } = vi.hoisted(() => ({
	mockSession: {
		canManage: true
	},
	mockImuState: {
		settings: {
			ui_monitor_enabled: true,
			alarm_monitor_enabled: true,
			orientation_baseline_valid: true,
			orientation_baseline: { x: 0, y: 0, z: 1 },
			baseline_calibrated_at: 1200,
			calibration_revision: 3,
			tilt_threshold_deg: 30,
			tilt_hysteresis_deg: 5,
			tilt_hold_ms: 750,
			tilt_clear_hold_ms: 1500,
			accel_delta_threshold_g: 0.35
		},
		loading: false,
		saving: false,
		hasChanges: true,
		errorMessage: null,
		status: {
			initialized: true,
			running: true,
			transition_in_progress: false,
			desired_mask: 0,
			running_mask: 0,
			desired_consumers: 'AirMouseMovement,AirMouseClick',
			running_consumers: 'AirMouseMovement',
			last_start_error: 'none',
			last_start_attempt_ms: 0,
			last_start_duration_ms: 0,
			next_retry_ms: 0,
			retry_pending: false,
			sample_fresh: true,
			sample_age_ms: 4,
			last_sample_ms: 100,
			consumers: {
				ui_monitor: { desired: true, running: true },
				alarm: { desired: true, running: true },
				auto_rotate: { desired: false, running: false },
				airmouse_movement: { desired: true, running: true },
				airmouse_click: { desired: true, running: false }
			},
			alarm: {
				enabled: true,
				sample_fresh: true,
				baseline_valid: true,
				triggered: false,
				pending_trigger: false,
				pending_clear: false,
				reason: 'none',
				tilt_deg: 2,
				accel_delta_g: 0.05,
				trigger_value: 0,
				trigger_hold_elapsed_ms: 0,
				clear_hold_elapsed_ms: 0
			},
			metrics: {
				accel_magnitude_g: 1,
				accel_delta_g: 0.05,
				gyro_magnitude_dps: 0.4,
				baseline_valid: true,
				orientation_baseline: { x: 0, y: 0, z: 1 },
				tilt_deg: 2,
				sample: { ax: 0, ay: 0, az: 1, gx: 0, gy: 0, gz: 0 }
			}
		},
		statusLoading: false,
		calibrating: false,
		resettingBaseline: false,
		calibrationResult: null,
		updateSetting: vi.fn(),
		saveSettings: vi.fn(),
		resetSettings: vi.fn(),
		refreshStatus: vi.fn(),
		calibrateOrientation: vi.fn(),
		resetOrientationBaseline: vi.fn()
	},
	mockConsumerSettings: {
		matrixAutoRotate: false,
		airMouseMovementEnabled: true,
		airMouseClickEnabled: true,
		airMouseRunning: true,
		loading: false,
		matrixSaving: false,
		errorMessage: null,
		refreshConsumers: vi.fn(),
		setMatrixAutoRotate: vi.fn(async () => true)
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
	access_admin_required_desc: () => 'Admin access is required.',
	access_admin_required_title: () => 'Admin required',
	action_discard: () => 'Discard',
	action_save: () => 'Save',
	common_loading: () => 'Loading',
	settings_title: () => 'Settings',
	imu_overview: () => 'IMU Overview',
	imu_alarm_orientation: () => 'Alarm & Orientation',
	imu_measurements: () => 'Measurements',
	imu_thresholds: () => 'Thresholds',
	imu_consumers: () => 'Consumers',
	imu_calibration: () => 'Orientation Baseline',
	imu_settings: () => 'Sensitivity',
	imu_metric_accel: () => 'Accel',
	imu_metric_delta: () => 'Delta',
	imu_metric_gyro: () => 'Gyro',
	imu_metric_tilt: () => 'Tilt',
	imu_metric_baseline: () => 'Baseline',
	imu_metric_revision: () => 'Revision',
	imu_running: () => 'Running',
	imu_initialized: () => 'Initialized',
	imu_sample: () => 'Sample',
	imu_sample_fresh: () => 'fresh',
	imu_sample_stale: () => 'stale',
	imu_last_error: () => 'Last start error',
	imu_retry_at: ({ duration }: { duration: string }) => `retry @ ${duration}`,
	imu_baseline_valid: () => 'valid',
	imu_baseline_missing: () => 'missing',
	imu_consumer_configured: () => 'Configured',
	imu_consumer_desired: () => 'Desired',
	imu_consumer_running: () => 'Running',
	imu_live_monitor: () => 'Live monitor',
	imu_live_monitor_desc: () => 'Keeps IMU sampling active for this page.',
	imu_alarm_monitor: () => 'Alarm monitor',
	imu_alarm_monitor_desc: () => 'Reserves IMU sampling for alarm evaluation.',
	imu_tilt_threshold: () => 'Tilt threshold',
	imu_tilt_hysteresis: () => 'Tilt hysteresis',
	imu_accel_delta_threshold: () => 'Acceleration delta',
	imu_tilt_hold: () => 'Tilt hold',
	imu_tilt_clear_hold: () => 'Clear hold',
	imu_calibrate_orientation: () => 'Calibrate',
	imu_reset_orientation: () => 'Reset',
	imu_calibration_success: () => 'Baseline calibrated',
	imu_calibration_failed: () => 'Calibration rejected',
	imu_alarm_state: () => 'Alarm',
	imu_alarm_triggered: () => 'triggered',
	imu_alarm_clear: () => 'clear',
	imu_alarm_reason: () => 'Reason',
	imu_current_tilt: () => 'current tilt',
	imu_current_accel_delta: () => 'current delta',
	imu_alarm_trigger_hold: () => 'trigger hold',
	imu_alarm_clear_hold: () => 'clear hold',
	imu_alarm_reason_none: () => 'none',
	imu_alarm_reason_tilt: () => 'tilt',
	imu_alarm_reason_shock: () => 'shock',
	imu_alarm_reason_stale: () => 'stale',
	imu_alarm_reason_no_baseline: () => 'no baseline',
	imu_alarm_reason_unavailable: () => 'unavailable',
	imu_consumer_ui_monitor: () => 'UI monitor',
	imu_consumer_alarm: () => 'Alarm monitor',
	imu_consumer_matrix: () => 'Matrix auto-rotate',
	imu_consumer_airmouse_movement: () => 'AirMouse movement',
	imu_consumer_airmouse_click: () => 'AirMouse click',
	imu_state_enabled: () => 'Enabled',
	imu_state_disabled: () => 'Disabled',
	imu_state_unknown: () => 'Unknown',
	imu_state_desired: () => 'desired',
	imu_state_not_desired: () => 'not desired',
	imu_state_running: () => 'running',
	imu_state_idle: () => 'idle',
	imu_open_config: () => 'Config',
	imu_samples: ({ count }: { count: string }) => `${count} samples`
}));

vi.mock('./useImuSettings.svelte', () => ({
	useImuSettings: () => mockImuState
}));

vi.mock('./useImuConsumerSettings.svelte', () => ({
	useImuConsumerSettings: () => mockConsumerSettings
}));

function getCardByTitle(title: string) {
	const titleElement = screen.getByText(title);
	const card = titleElement.closest('.card');

	expect(card).toBeTruthy();

	return card as HTMLElement;
}

function getConsumerRow(label: string) {
	return screen.getByTestId(`imu-consumer-${label}`);
}

describe('ImuPage', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockSession.canManage = true;
		mockImuState.loading = false;
		mockImuState.saving = false;
		mockImuState.hasChanges = true;
		mockConsumerSettings.airMouseMovementEnabled = true;
		mockConsumerSettings.airMouseClickEnabled = true;
		mockConsumerSettings.errorMessage = null;
	});

	afterEach(() => {
		cleanup();
	});

	it('groups IMU status into two cards and keeps Save local to settings', () => {
		render(ImuPage);

		expect(document.querySelectorAll('.card')).toHaveLength(2);
		expect(getCardByTitle('IMU Overview')).toBeTruthy();
		const settingsCard = getCardByTitle('Alarm & Orientation');

		expect(screen.getAllByRole('button', { name: 'Save' })).toHaveLength(1);
		expect(within(settingsCard).getByRole('button', { name: 'Save' })).toBeTruthy();
		expect(
			within(getCardByTitle('IMU Overview')).queryByRole('button', { name: 'Save' })
		).toBeNull();
	});

	it('renders IMU millisecond durations as seconds', () => {
		render(ImuPage);

		const settingsCard = getCardByTitle('Alarm & Orientation');
		expect(within(settingsCard).getByText('0.75 s')).toBeTruthy();
		expect(within(settingsCard).getByText('1.5 s')).toBeTruthy();
		expect(within(settingsCard).getByText('1.2 s')).toBeTruthy();
		expect(within(settingsCard).queryByText(/750 ms/)).toBeNull();
		expect(within(settingsCard).queryByText(/1500 ms/)).toBeNull();
	});

	it('labels AirMouse consumer state as configured, desired, and running', () => {
		render(ImuPage);

		for (const label of ['airmouse_movement', 'airmouse_click']) {
			const row = getConsumerRow(label);

			expect(within(row).getByText('Configured:')).toBeTruthy();
			expect(within(row).getByText('Desired:')).toBeTruthy();
			expect(within(row).getByText('Running:')).toBeTruthy();
		}

		const movementRow = getConsumerRow('airmouse_movement');
		expect(within(movementRow).getByText('Enabled')).toBeTruthy();
		expect(within(movementRow).getByText('desired')).toBeTruthy();
		expect(within(movementRow).getByText('running')).toBeTruthy();

		const clickRow = getConsumerRow('airmouse_click');
		expect(within(clickRow).getByText('Enabled')).toBeTruthy();
		expect(within(clickRow).getByText('desired')).toBeTruthy();
		expect(within(clickRow).getByText('idle')).toBeTruthy();
	});
});
