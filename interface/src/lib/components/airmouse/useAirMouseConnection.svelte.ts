import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import { useSystemTransportState } from '$lib/stores/system/transportState.svelte';
import type { SystemEvent } from '$lib/stores/system/types';
import type { AirMouseStatus } from '$lib/types/devices/airmouse';
import { SocketWatchdog } from '$lib/utils/ws/socketWatchdog';
import { AIR_MOUSE_WS } from './airMouseConfig';
import {
	buildUpdatedAirMouseStatus,
	isAirMouseImuNeeded,
	pushHistoryValue
} from './airMouseTelemetry';

interface AirMouseSystemStatusStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
	getSnapshot<TSnapshot>(channel: string): TSnapshot | null;
	requestSnapshot?(channel: string): void;
	subscribeEvents?(run: (value: SystemEvent | null) => void): () => void;
}

interface AirMouseSystemEvents {
	subscribe(run: (value: SystemEvent | null) => void): () => void;
}

interface AirMouseTransportState {
	readonly isConnected: boolean;
}

interface AirMouseConnectionDeps {
	systemStatusStore?: AirMouseSystemStatusStore;
	systemEventsBus?: AirMouseSystemEvents;
	transportState?: AirMouseTransportState;
}

export function useAirMouseConnection(
	getStatus: () => AirMouseStatus | null,
	setStatus: (status: AirMouseStatus | null) => void,
	deps: AirMouseConnectionDeps = {}
) {
	const systemStatusStore = deps.systemStatusStore;
	const systemEventsBus = deps.systemEventsBus;
	const transportState =
		deps.transportState ??
		(deps.systemStatusStore as AirMouseTransportState | undefined) ??
		useSystemTransportState();

	let wsConnected = $state(false);
	let deltaGHistory = $state<number[]>([]);
	let gyroXHistory = $state<number[]>([]);
	let gyroZHistory = $state<number[]>([]);
	let maxHistory = $state<number>(AIR_MOUSE_WS.maxHistory);
	let uiRequestingConnection = $state(false);

	let subscriptionActive = false;

	const telemetrySubscription = createSystemChannelSubscription<never>({
		channel: 'airmouse',
		systemStatusStore,
		systemEventsBus,
		onEvent: (event: SystemEvent) => {
			if (!subscriptionActive || event.type !== 'airmouse') {
				return;
			}

			resetWatchdog();
			const sample = {
				gx: event.data.gx,
				gy: event.data.gy,
				gz: event.data.gz,
				ax: event.data.ax,
				ay: event.data.ay,
				az: event.data.az,
				deltaG: event.data.deltaG
			};

			setStatus(buildUpdatedAirMouseStatus(getStatus(), sample));
			deltaGHistory = pushHistoryValue(deltaGHistory, sample.deltaG, maxHistory);
			gyroXHistory = pushHistoryValue(gyroXHistory, sample.gx, maxHistory);
			gyroZHistory = pushHistoryValue(gyroZHistory, sample.gz, maxHistory);
		}
	});

	$effect(() => {
		const connected = transportState.isConnected;
		wsConnected = subscriptionActive && connected;
	});

	const watchdog = new SocketWatchdog({
		timeoutMs: AIR_MOUSE_WS.watchdogTimeoutMs,
		onTimeout: () => {
			if (!subscriptionActive) return;

			console.warn('[AirMouse] Channel watchdog timeout - refreshing subscription');
			refreshChannelSubscription();
		}
	});

	function init() {
		updateConnectionState();
	}

	function destroy() {
		uiRequestingConnection = false;
		stopWebSocket();
		watchdog.destroy();
	}

	function updateConnectionState() {
		const shouldConnect = uiRequestingConnection && isAirMouseImuNeeded(getStatus());

		if (shouldConnect && !subscriptionActive) {
			startChannelSubscription();
		} else if (!shouldConnect && subscriptionActive) {
			stopWebSocket();
		}
	}

	function startChannelSubscription() {
		if (subscriptionActive) return;

		subscriptionActive = true;
		telemetrySubscription.subscribe({ hydrateSnapshot: false });
		resetWatchdog();
	}

	function refreshChannelSubscription() {
		if (!subscriptionActive) return;

		telemetrySubscription.unsubscribe();
		telemetrySubscription.subscribe({ hydrateSnapshot: false });
		resetWatchdog();
	}

	function stopWebSocket() {
		clearWatchdog();
		subscriptionActive = false;
		telemetrySubscription.destroy();

		wsConnected = false;
	}

	function resetHistory() {
		deltaGHistory = [];
		gyroXHistory = [];
		gyroZHistory = [];
	}

	function resetWatchdog() {
		watchdog.arm(subscriptionActive);
	}

	function clearWatchdog() {
		watchdog.clear();
	}

	return {
		get wsConnected() {
			return wsConnected;
		},
		get deltaGHistory() {
			return deltaGHistory;
		},
		get gyroXHistory() {
			return gyroXHistory;
		},
		get gyroZHistory() {
			return gyroZHistory;
		},
		get maxHistory() {
			return maxHistory;
		},
		get uiRequestingConnection() {
			return uiRequestingConnection;
		},
		set uiRequestingConnection(value: boolean) {
			uiRequestingConnection = value;
		},
		init,
		destroy,
		updateConnectionState,
		stopWebSocket,
		resetHistory
	};
}
