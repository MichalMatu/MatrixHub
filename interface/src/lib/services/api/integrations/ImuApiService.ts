import { createApiClient, type ApiClientOptions } from '$lib/utils';

export interface ImuVector3 {
	x: number;
	y: number;
	z: number;
}

export interface ImuSettings {
	ui_monitor_enabled: boolean;
	alarm_monitor_enabled: boolean;
	orientation_baseline_valid: boolean;
	orientation_baseline: ImuVector3;
	baseline_calibrated_at: number;
	calibration_revision: number;
	tilt_threshold_deg: number;
	tilt_hysteresis_deg: number;
	tilt_hold_ms: number;
	tilt_clear_hold_ms: number;
	accel_delta_threshold_g: number;
}

export interface ImuConsumerState {
	desired: boolean;
	running: boolean;
}

interface ImuSample {
	ax: number;
	ay: number;
	az: number;
	gx: number;
	gy: number;
	gz: number;
}

export interface ImuMetrics {
	accel_magnitude_g: number;
	accel_delta_g: number;
	gyro_magnitude_dps: number;
	baseline_valid: boolean;
	orientation_baseline: ImuVector3;
	tilt_deg: number | null;
	sample: ImuSample;
}

export interface ImuAlarmStatus {
	enabled: boolean;
	sample_fresh: boolean;
	baseline_valid: boolean;
	triggered: boolean;
	pending_trigger: boolean;
	pending_clear: boolean;
	reason: 'none' | 'tilt' | 'shock' | 'stale' | 'no_baseline' | 'unavailable' | 'unknown';
	tilt_deg: number | null;
	accel_delta_g: number;
	trigger_value: number;
	trigger_hold_elapsed_ms: number;
	clear_hold_elapsed_ms: number;
}

export interface ImuStatus {
	initialized: boolean;
	running: boolean;
	transition_in_progress: boolean;
	desired_mask: number;
	running_mask: number;
	desired_consumers: string;
	running_consumers: string;
	last_start_error: string;
	last_start_attempt_ms: number;
	last_start_duration_ms: number;
	next_retry_ms: number;
	retry_pending: boolean;
	sample_fresh: boolean;
	sample_age_ms: number | null;
	last_sample_ms: number | null;
	consumers: Record<
		| 'airmouse_movement'
		| 'airmouse_click'
		| 'auto_rotate'
		| 'alarm'
		| 'ui_monitor'
		| 'matrix_effects',
		ImuConsumerState
	>;
	alarm: ImuAlarmStatus;
	metrics: ImuMetrics;
}

export interface ImuCalibrationResult {
	ok: boolean;
	status: string;
	sample_count: number;
	accel_magnitude_mean: number;
	accel_magnitude_variance: number;
	orientation_baseline: ImuVector3;
}

export interface ImuResetResult {
	ok: boolean;
	status: string;
}

export const DEFAULT_IMU_SETTINGS: ImuSettings = {
	ui_monitor_enabled: false,
	alarm_monitor_enabled: false,
	orientation_baseline_valid: false,
	orientation_baseline: { x: 0, y: 0, z: 1 },
	baseline_calibrated_at: 0,
	calibration_revision: 0,
	tilt_threshold_deg: 30,
	tilt_hysteresis_deg: 5,
	tilt_hold_ms: 750,
	tilt_clear_hold_ms: 1500,
	accel_delta_threshold_g: 0.35
};

export class ImuApiService {
	private client;
	private static readonly GET_TIMEOUT_MS = 5000;
	private static readonly UPDATE_TIMEOUT_MS = 8000;
	private static readonly CALIBRATE_TIMEOUT_MS = 20000;

	constructor(options: ApiClientOptions) {
		this.client = createApiClient(options);
	}

	async getStatus(signal?: AbortSignal): Promise<ImuStatus> {
		return this.client.get<ImuStatus>('/api/imu/status', {
			signal: signal
				? AbortSignal.any([signal, AbortSignal.timeout(ImuApiService.GET_TIMEOUT_MS)])
				: AbortSignal.timeout(ImuApiService.GET_TIMEOUT_MS)
		});
	}

	async getSettings(): Promise<ImuSettings> {
		return this.client.get<ImuSettings>('/api/imu/settings', {
			signal: AbortSignal.timeout(ImuApiService.GET_TIMEOUT_MS)
		});
	}

	async updateSettings(settings: ImuSettings): Promise<ImuSettings> {
		return this.client.post<ImuSettings>('/api/imu/settings', settings, {
			signal: AbortSignal.timeout(ImuApiService.UPDATE_TIMEOUT_MS)
		});
	}

	async calibrateOrientation(): Promise<ImuCalibrationResult> {
		return this.client.post<ImuCalibrationResult>(
			'/api/imu/calibrate/orientation',
			{},
			{ signal: AbortSignal.timeout(ImuApiService.CALIBRATE_TIMEOUT_MS) }
		);
	}

	async resetOrientationBaseline(): Promise<ImuResetResult> {
		return this.client.post<ImuResetResult>(
			'/api/imu/reset/orientation-baseline',
			{},
			{ signal: AbortSignal.timeout(ImuApiService.UPDATE_TIMEOUT_MS) }
		);
	}
}
