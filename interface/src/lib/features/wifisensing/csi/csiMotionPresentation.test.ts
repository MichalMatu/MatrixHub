import { describe, expect, it } from 'vitest';
import type { CsiMotionStatus } from '$lib/types/connectivity/wifiSensing';
import {
	canCalibrateCsiMotion,
	resolveCsiMotionBadgeClass,
	resolveCsiMotionPresentationState
} from './csiMotionPresentation';

const monitoring: CsiMotionStatus = {
	enabled: true,
	state: 'monitoring',
	baseline_ready: true,
	detected: false,
	decision_valid: true,
	has_frame: true,
	data_fresh: true,
	last_frame_ms: 1000,
	frame_age_ms: 10,
	noisy: false,
	needs_calibration: false,
	score: 0,
	confidence: 0,
	frames_seen: 100,
	width: 192,
	band_count: 1,
	selected_carriers: 16,
	valid_carriers: 192,
	last_reset_reason: 'startup'
};

describe('resolveCsiMotionPresentationState', () => {
	it('keeps the backend disabled state authoritative over an enabled local draft', () => {
		expect(
			resolveCsiMotionPresentationState(true, {
				...monitoring,
				enabled: false,
				state: 'disabled',
				has_frame: false,
				data_fresh: false
			})
		).toBe('disabled');
		expect(resolveCsiMotionBadgeClass('disabled')).toBe('badge-ghost');
	});

	it('prioritizes required calibration over retained motion', () => {
		expect(
			resolveCsiMotionPresentationState(true, {
				...monitoring,
				state: 'needs_calibration',
				detected: true,
				decision_valid: false,
				needs_calibration: true
			})
		).toBe('needs_calibration');
	});

	it('keeps ordinary automatic baseline collection in calibrating state', () => {
		expect(
			resolveCsiMotionPresentationState(true, {
				...monitoring,
				state: 'calibrating',
				baseline_ready: false,
				decision_valid: false,
				needs_calibration: false
			})
		).toBe('calibrating');
	});

	it('prioritizes stale data over a retained motion bit', () => {
		expect(
			resolveCsiMotionPresentationState(true, {
				...monitoring,
				state: 'unavailable',
				detected: true,
				decision_valid: false,
				data_fresh: false
			})
		).toBe('stale');
	});

	it('reports fresh definitive motion normally', () => {
		expect(resolveCsiMotionPresentationState(true, { ...monitoring, detected: true })).toBe(
			'motion'
		);
	});
});

describe('canCalibrateCsiMotion', () => {
	const readyRuntime = {
		enabled: true,
		runtime_fault: false,
		runtime_reconcile_pending: false,
		calibration_state: 'forced',
		motion: {
			enabled: true,
			state: 'monitoring',
			has_frame: true,
			data_fresh: true,
			band_count: 1
		}
	};

	it('requires a live forced-gain runtime and a fresh valid frame', () => {
		expect(canCalibrateCsiMotion(readyRuntime)).toBe(true);
		expect(canCalibrateCsiMotion({ ...readyRuntime, runtime_fault: true })).toBe(false);
		expect(
			canCalibrateCsiMotion({
				...readyRuntime,
				motion: { ...readyRuntime.motion, state: 'unavailable' }
			})
		).toBe(false);
		expect(
			canCalibrateCsiMotion({
				...readyRuntime,
				motion: { ...readyRuntime.motion, data_fresh: false }
			})
		).toBe(false);
	});

	it('uses applied runtime bands and rejects dirty or saving drafts', () => {
		expect(
			canCalibrateCsiMotion({
				...readyRuntime,
				motion: { ...readyRuntime.motion, band_count: 0 }
			})
		).toBe(false);
		expect(canCalibrateCsiMotion(readyRuntime, true, false)).toBe(false);
		expect(canCalibrateCsiMotion(readyRuntime, false, true)).toBe(false);
	});
});
