import type { CsiMotionStatus, CsiRuntimeMetrics } from '$lib/types/connectivity/wifiSensing';

type CsiCalibrationRuntime = Pick<
	CsiRuntimeMetrics,
	'enabled' | 'runtime_fault' | 'runtime_reconcile_pending' | 'calibration_state'
> & {
	motion: Pick<CsiMotionStatus, 'enabled' | 'state' | 'has_frame' | 'data_fresh' | 'band_count'>;
};

export type CsiMotionPresentationState =
	| 'disabled'
	| 'unknown'
	| 'needs_calibration'
	| 'stale'
	| 'motion'
	| 'noisy'
	| 'calibrating'
	| 'needs_configuration'
	| 'monitoring';

export function resolveCsiMotionPresentationState(
	enabled: boolean,
	status: CsiMotionStatus | null
): CsiMotionPresentationState {
	if (!enabled) return 'disabled';
	if (!status) return 'unknown';
	if (!status.enabled || status.state === 'disabled') return 'disabled';
	if (status.needs_calibration || status.state === 'needs_calibration') {
		return 'needs_calibration';
	}
	if (status.state === 'unavailable' || (status.has_frame && !status.data_fresh)) return 'stale';
	if (status.noisy) return 'noisy';
	if (status.detected) return 'motion';
	if (status.state === 'calibrating') return 'calibrating';
	if (status.state === 'needs_configuration') return 'needs_configuration';
	return 'monitoring';
}

export function resolveCsiMotionBadgeClass(state: CsiMotionPresentationState): string {
	if (state === 'motion') return 'badge-error';
	if (['needs_calibration', 'stale', 'noisy'].includes(state)) return 'badge-warning';
	if (state === 'disabled' || state === 'unknown') return 'badge-ghost';
	return 'badge-success';
}

export function canCalibrateCsiMotion(
	runtime: CsiCalibrationRuntime | null,
	hasUnsavedChanges = false,
	saving = false
): boolean {
	return !!(
		!hasUnsavedChanges &&
		!saving &&
		runtime?.enabled &&
		!runtime.runtime_fault &&
		!runtime.runtime_reconcile_pending &&
		runtime.calibration_state === 'forced' &&
		runtime.motion.enabled &&
		runtime.motion.band_count > 0 &&
		runtime.motion.state !== 'unavailable' &&
		runtime.motion.has_frame &&
		runtime.motion.data_fresh
	);
}
