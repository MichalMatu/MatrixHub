import type { SystemInformation } from '$lib/types/system/system';
import type { HealthDiagnostics } from '$lib/types/system/health';
import type { WifiMode } from '$lib/types/connectivity/wifi';

export type SystemStatusWifiDiagnostics = HealthDiagnostics['wifi'] & {
	lastDisconnectReason: number;
	healthy: boolean;
	state?: string;
	configuredMode?: WifiMode;
	mode?: string;
	apActive?: boolean;
	lastRecoveryReason?: string;
	lastIpChangeMs?: number;
	disconnectedSinceMs?: number;
	stableConnectedSinceMs?: number;
	savedStaticIp?: string;
	mac?: string;
	channel?: number;
	bssid?: string;
	gateway?: string;
	subnet?: string;
	dns?: string;
};

export interface SystemStatusApDiagnostics {
	active?: boolean;
	stationNum?: number;
	ip?: string;
	mac?: string;
}

export interface SystemStatusHttpDiagnostics {
	activeClients?: number;
	peakClients?: number;
	opens?: number;
	closes?: number;
	lastOpenMs?: number;
	lastCloseMs?: number;
	wsActiveClients?: number;
	wsPeakClients?: number;
	wsOpens?: number;
	wsCloses?: number;
	lastWsOpenMs?: number;
	lastWsCloseMs?: number;
	wsForcedRemovals?: number;
	wsQueueDrops?: number;
	lastWsQueueDropMs?: number;
	lastWsQueueDropPayload?: number;
	wsHeapFallbacks?: number;
	lastWsHeapFallbackMs?: number;
	lastWsHeapFallbackPayload?: number;
	maxWsHeapFallbackPayload?: number;
}

export interface SystemStatusForwardingDiagnostics {
	ready?: boolean;
	requiresStaticIp?: boolean;
	savedStaticIpConfigured?: boolean;
	savedStaticIpMatches?: boolean;
	httpsPort?: number;
}

export interface DashboardBleWidgetSummary {
	enabled: boolean;
	sensor_count: number;
}

export interface DashboardShellyWidgetSummary {
	device_count: number;
}

export interface DashboardAlarmsWidgetSummary {
	rule_count: number;
}

export interface DashboardWifiSensingWidgetSummary {
	enabled: boolean;
}

export interface SystemStatusDashboardWidgetsSummary {
	ble: DashboardBleWidgetSummary;
	shelly: DashboardShellyWidgetSummary;
	alarms: DashboardAlarmsWidgetSummary;
	wifi_sensing: DashboardWifiSensingWidgetSummary;
}

export interface ExtendedHealthDiagnostics extends Omit<HealthDiagnostics, 'wifi'> {
	wifi: SystemStatusWifiDiagnostics;
	ap?: SystemStatusApDiagnostics;
	http?: SystemStatusHttpDiagnostics;
	forwarding?: SystemStatusForwardingDiagnostics;
}

export interface SystemStatusSnapshot {
	system_info?: SystemInformation;
	diagnostics?: ExtendedHealthDiagnostics;
	dashboard_widgets?: SystemStatusDashboardWidgetsSummary;
	wifi_ap_mode?: boolean;
	config?: { logging?: { level?: string } };
}
