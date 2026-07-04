import { untrack } from 'svelte';
import { createSystemChannelSubscription } from '$lib/stores/system/channelSubscription.svelte';
import type {
	SystemStatusDashboardWidgetsSummary,
	SystemStatusSnapshot
} from '$lib/types/system/systemStatusSnapshot';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';

export interface DashboardWidgetVisibility {
	showBle: boolean;
	showShelly: boolean;
	showAlarms: boolean;
	showWifiSensing: boolean;
}

const DEFAULT_VISIBILITY: DashboardWidgetVisibility = {
	showBle: false,
	showShelly: false,
	showAlarms: false,
	showWifiSensing: false
};

const LEGACY_FALLBACK_VISIBILITY: DashboardWidgetVisibility = {
	showBle: true,
	showShelly: true,
	showAlarms: true,
	showWifiSensing: true
};

export function resolveDashboardWidgetVisibility(
	summaryLoaded: boolean,
	summary: SystemStatusDashboardWidgetsSummary | null
): DashboardWidgetVisibility {
	if (!summaryLoaded) {
		return DEFAULT_VISIBILITY;
	}

	if (!summary) {
		return LEGACY_FALLBACK_VISIBILITY;
	}

	return {
		showBle: summary.ble.enabled || summary.ble.sensor_count > 0,
		showShelly: summary.shelly.device_count > 0,
		showAlarms: summary.alarms.rule_count > 0,
		showWifiSensing: summary.wifi_sensing.enabled
	};
}

export function useDashboardWidgetVisibility() {
	let summary = $state<SystemStatusDashboardWidgetsSummary | null>(null);
	let summaryLoaded = $state(false);

	const session = useSessionAccess();

	function applySnapshot(snapshot: SystemStatusSnapshot) {
		summary = snapshot.dashboard_widgets ?? null;
		summaryLoaded = true;
	}

	function resetSnapshot() {
		summary = null;
		summaryLoaded = false;
	}

	const systemStatusChannel = createSystemChannelSubscription<SystemStatusSnapshot>({
		channel: 'system_status',
		onSnapshot: applySnapshot,
		onReset: resetSnapshot
	});

	$effect(() => {
		if (!session.canRead) {
			summary = null;
			summaryLoaded = false;
			return;
		}

		untrack(() => {
			systemStatusChannel.subscribe();
		});

		return () => {
			untrack(() => {
				systemStatusChannel.destroy();
			});
		};
	});

	const visibility = $derived(resolveDashboardWidgetVisibility(summaryLoaded, summary));
	const hasVisibilityDecision = $derived(summaryLoaded);
	return {
		get hasVisibilityDecision() {
			return hasVisibilityDecision;
		},
		get showBle() {
			return visibility.showBle;
		},
		get showShelly() {
			return visibility.showShelly;
		},
		get showAlarms() {
			return visibility.showAlarms;
		},
		get showWifiSensing() {
			return visibility.showWifiSensing;
		}
	};
}
