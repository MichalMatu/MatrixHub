import type { RssiSample, WifiSensingStats } from '$lib/types/connectivity/wifiSensing';

const MAX_WIFI_SENSING_SAMPLES = 500;

export function appendWifiSensingSample(
	samples: RssiSample[],
	sample: RssiSample,
	maxSamples: number = MAX_WIFI_SENSING_SAMPLES
): RssiSample[] {
	const nextSamples = [...samples, sample];
	if (nextSamples.length <= maxSamples) {
		return nextSamples;
	}

	return nextSamples.slice(nextSamples.length - maxSamples);
}

export function createInitialWifiSensingStats(rssi: number): WifiSensingStats {
	return {
		current: rssi,
		min: rssi,
		max: rssi,
		avg: rssi,
		variance: 0,
		sampleCount: 0,
		windowMs: 0
	};
}

export function calculateWifiSensingStats(samples: RssiSample[]): WifiSensingStats {
	if (samples.length === 0) {
		return createInitialWifiSensingStats(0);
	}

	let min = samples[0].rssi;
	let max = samples[0].rssi;
	let sum = 0;

	for (const sample of samples) {
		if (sample.rssi < min) min = sample.rssi;
		if (sample.rssi > max) max = sample.rssi;
		sum += sample.rssi;
	}

	const first = samples[0];
	const last = samples[samples.length - 1];

	return {
		current: last.rssi,
		min,
		max,
		avg: sum / samples.length,
		variance: last.variance ?? 0,
		sampleCount: samples.length,
		windowMs: samples.length > 1 ? last.timestamp - first.timestamp : 0
	};
}
