import { untrack } from 'svelte';
import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import type { SystemEvent } from '$lib/stores/system/types';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

const VARIANCE_THRESHOLD_COLOR_TRANSITION = 0.6;

export function useWifiSensingWidgetManagement() {
	let enabled = $state(false);
	let hasData = $state(false);
	let motionDetected = $state(false);
	let variance = $state(0);
	let varianceThreshold = $state(4.0);
	let rssi = $state(0);
	let lastUpdateTime = $state<Date | null>(null);
	let loading = $state(true);
	let errorMessage = $state<string | null>(null);

	const session = useSessionAccess();

	function resetLiveData() {
		hasData = false;
		motionDetected = false;
		variance = 0;
		rssi = 0;
		lastUpdateTime = null;
	}

	function handleSensingEvent(event: SystemEvent | null) {
		if (event?.type !== 'sensing') return;

		enabled = true;
		hasData = true;
		rssi = event.data.rssi;
		variance = event.data.variance;
		motionDetected = event.data.motion;
		lastUpdateTime = new Date();
		errorMessage = null;
		loading = false;
	}

	function handleSensingSnapshot(settings: { enabled: boolean; variance_threshold?: number }) {
		enabled = settings.enabled;
		if (settings.variance_threshold !== undefined) {
			varianceThreshold = settings.variance_threshold;
		}
		if (!settings.enabled) {
			resetLiveData();
		}
		errorMessage = null;
		loading = false;
	}

	const sensingChannel = createSystemChannelSubscription<{
		enabled: boolean;
		variance_threshold?: number;
	}>({
		channel: 'sensing',
		onSnapshot: handleSensingSnapshot,
		onEvent: handleSensingEvent
	});

	$effect(() => {
		untrack(() => {
			sensingChannel.subscribe();
		});

		if (!session.canRead) {
			enabled = false;
			resetLiveData();
			loading = false;
		}

		return () => {
			untrack(() => {
				sensingChannel.destroy();
			});
		};
	});

	function getVarianceColor(value: number, threshold: number): string {
		if (value < threshold * VARIANCE_THRESHOLD_COLOR_TRANSITION) return 'text-success';
		if (value < threshold) return 'text-warning';
		return 'text-error';
	}

	function formatTime(date: Date | null): string {
		if (!date) return '--:--:--';
		return date.toLocaleTimeString('pl-PL', {
			hour: '2-digit',
			minute: '2-digit',
			second: '2-digit'
		});
	}

	return {
		get enabled() {
			return enabled;
		},
		get hasData() {
			return hasData;
		},
		get motionDetected() {
			return motionDetected;
		},
		get variance() {
			return variance;
		},
		get varianceThreshold() {
			return varianceThreshold;
		},
		get rssi() {
			return rssi;
		},
		get lastUpdateTime() {
			return lastUpdateTime;
		},
		get loading() {
			return loading;
		},
		get errorMessage() {
			return errorMessage;
		},
		getVarianceColor,
		formatTime
	};
}
