import type { AirMouseImuData, AirMouseStatus } from '$lib/types/devices/airmouse';
import { AIR_MOUSE_CLICK_SOURCE } from './airMouseConfig';

export interface AirMouseTelemetrySample {
	gx: number;
	gy: number;
	gz: number;
	ax: number;
	ay: number;
	az: number;
	deltaG: number;
}

export function isAirMouseImuNeeded(status: AirMouseStatus | null): boolean {
	return (
		!!status &&
		(status.movement_enabled ||
			(status.click_enabled && status.click_source === AIR_MOUSE_CLICK_SOURCE.SENSOR))
	);
}

export function buildUpdatedAirMouseStatus(
	status: AirMouseStatus | null,
	sample: AirMouseTelemetrySample
): AirMouseStatus | null {
	if (!status) return null;

	const imu: AirMouseImuData = status.imu ?? {
		gx: 0,
		gy: 0,
		gz: 0,
		ax: 0,
		ay: 0,
		az: 0
	};

	return {
		...status,
		last_delta_g: sample.deltaG,
		imu: {
			...imu,
			gx: sample.gx,
			gy: sample.gy,
			gz: sample.gz,
			ax: sample.ax,
			ay: sample.ay,
			az: sample.az
		}
	};
}

export function pushHistoryValue(history: number[], value: number, maxHistory: number): number[] {
	if (maxHistory <= 0) return [];

	const nextHistory =
		history.length >= maxHistory ? [...history.slice(1), value] : [...history, value];

	return nextHistory;
}
