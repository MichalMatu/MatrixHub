import type { WifiStatus } from '$lib/types/connectivity/wifi';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { createSystemStatusBackedStatus } from '../systemStatusBackedStatus.svelte';
import type { SystemStatusSnapshot } from '$lib/types/system/systemStatusSnapshot';

const ERROR_COOLDOWN_MS = 12000;

const DEFAULT_STATUS: WifiStatus = {
	status: 0,
	ssid: '',
	local_ip: '',
	mac_address: '',
	rssi: 0,
	bssid: '',
	channel: 0,
	gateway_ip: '',
	subnet_mask: '',
	dns_ip_1: ''
};

function mapWifiStatusSnapshot(snapshot: SystemStatusSnapshot): WifiStatus | null {
	const wifi = snapshot.diagnostics?.wifi;
	if (!wifi) return null;

	return {
		status: wifi.connected ? 3 : 0,
		ssid: wifi.ssid ?? '',
		local_ip: wifi.ip ?? '',
		mac_address: wifi.mac ?? '',
		rssi: wifi.rssi ?? 0,
		bssid: wifi.bssid ?? '',
		channel: wifi.channel ?? 0,
		gateway_ip: wifi.gateway ?? '',
		subnet_mask: wifi.subnet ?? '',
		dns_ip_1: wifi.dns ?? ''
	};
}

export function useWifiStatus() {
	const statusSync = createSystemStatusBackedStatus({
		defaultStatus: DEFAULT_STATUS,
		mapSnapshot: mapWifiStatusSnapshot,
		errorCooldownMs: ERROR_COOLDOWN_MS,
		fallbackMessage: () => m.toast_wifi_status_load_failed({ locale: i18n.languageTag }),
		loggerLabel: 'WiFi status'
	});

	return {
		get status() {
			return statusSync.status;
		},
		get error() {
			return statusSync.error;
		},
		start: statusSync.start,
		refresh: statusSync.refresh
	};
}
