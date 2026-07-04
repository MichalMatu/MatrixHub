import { Logger } from '$lib/services/core/Logger';
import type { WifiSensingApiService } from '$lib/services/api/connectivity/WifiSensingApiService';
import type { SensingEventData } from '$lib/stores/system/types';
import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import type { WifiSensingData, RssiSample } from '$lib/types/connectivity/wifiSensing';
import {
	appendWifiSensingSample,
	calculateWifiSensingStats,
	createInitialWifiSensingStats
} from './wifiSensingStats';

interface WifiSensingStatusOptions {
	isEnabled: () => boolean;
	getThreshold: () => number;
}

export function useWifiSensingStatus(
	getApi: () => WifiSensingApiService,
	options: WifiSensingStatusOptions
) {
	let sensingData = $state<WifiSensingData | null>(null);
	let samples = $state<RssiSample[]>([]);
	let lastUpdate = $state(Date.now());
	let isStarted = false;
	let connectionInfoRequestId = 0;

	const isActive = $derived(sensingData?.running ?? false);
	const motionDetected = $derived(sensingData?.motionDetected ?? false);

	function createEmptySensingData(rssi: number): WifiSensingData {
		return {
			enabled: true,
			running: true,
			active: true,
			motionDetected: false,
			connectedSSID: '',
			connectedChannel: 0,
			variance_threshold: options.getThreshold(),
			stats: createInitialWifiSensingStats(rssi)
		};
	}

	function applyConnectionInfo(ssid: string, channel: number, fallbackRssi: number) {
		const nextData = sensingData ?? createEmptySensingData(fallbackRssi);
		sensingData = {
			...nextData,
			connectedSSID: ssid,
			connectedChannel: channel,
			variance_threshold: options.getThreshold()
		};
	}

	function processSensingEvent(data: SensingEventData) {
		if (!options.isEnabled()) return;

		const now = Date.now();
		const nextSample: RssiSample = {
			rssi: data.rssi,
			timestamp: now,
			variance: data.variance
		};
		const nextSamples = appendWifiSensingSample(samples, nextSample);
		const nextStats = calculateWifiSensingStats(nextSamples);

		lastUpdate = now;
		samples = nextSamples;

		if (!sensingData) {
			sensingData = {
				enabled: true,
				running: true,
				active: true,
				motionDetected: data.motion,
				connectedSSID: '',
				connectedChannel: 0,
				variance_threshold: options.getThreshold(),
				stats: nextStats
			};

			void updateConnectionInfo();
			return;
		}

		sensingData = {
			...sensingData,
			enabled: true,
			running: true,
			active: true,
			motionDetected: data.motion,
			variance_threshold: options.getThreshold(),
			stats: nextStats
		};
	}

	const sensingChannel = createSystemChannelSubscription<never>({
		channel: 'sensing',
		onEvent: (event) => {
			if (event.type === 'sensing' && event.data) {
				processSensingEvent(event.data);
			}
		}
	});

	function start() {
		if (isStarted) return;
		if (!options.isEnabled()) return;

		isStarted = true;
		sensingChannel.subscribe({ hydrateSnapshot: false });
		void updateConnectionInfo();
	}

	function stop() {
		connectionInfoRequestId += 1;
		if (isStarted) {
			sensingChannel.unsubscribe();
			isStarted = false;
		}

		sensingData = null;
		samples = [];
	}

	async function updateConnectionInfo() {
		const requestId = ++connectionInfoRequestId;
		try {
			if (!isStarted || !options.isEnabled()) return;

			const wifiStatus = await getApi().getWifiStatus();

			if (requestId !== connectionInfoRequestId || !isStarted || !options.isEnabled()) return;
			if (
				wifiStatus.status !== 3 ||
				!wifiStatus.ssid ||
				wifiStatus.channel === undefined ||
				wifiStatus.rssi === undefined
			) {
				return;
			}

			applyConnectionInfo(wifiStatus.ssid, wifiStatus.channel, wifiStatus.rssi);
		} catch (nextError) {
			Logger.warn('Failed to fetch WiFi connection info for sensing context', nextError);
		}
	}

	function destroy() {
		stop();
		sensingChannel.destroy();
	}

	return {
		get sensingData() {
			return sensingData;
		},
		get samples() {
			return samples;
		},
		get lastUpdate() {
			return lastUpdate;
		},
		get isActive() {
			return isActive;
		},
		get motionDetected() {
			return motionDetected;
		},
		start,
		stop,
		destroy
	};
}
