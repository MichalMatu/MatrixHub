/**
 * WiFi Station (STA) types
 */

export type WifiStatus = {
	status: number;
	local_ip?: string;
	mac_address?: string;
	rssi?: number;
	ssid?: string;
	bssid?: string;
	channel?: number;
	subnet_mask?: string;
	gateway_ip?: string;
	dns_ip_1?: string;
	dns_ip_2?: string;
};

export type WifiMode = 'off' | 'ap' | 'sta';

export type WifiSettings = {
	hostname: string;
	mode: WifiMode;
	wifi_networks: KnownNetworkItem[];
};

export type KnownNetworkItem = {
	ssid: string;
	password: string;
	static_ip_config: boolean;
	local_ip?: string;
	subnet_mask?: string;
	gateway_ip?: string;
	dns_ip_1?: string;
	dns_ip_2?: string;
};

export type NetworkItem = {
	rssi: number;
	ssid: string;
	bssid: string;
	channel: number;
	encryption_type: number;
};
