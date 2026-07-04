/**
 * BLE (Bluetooth Low Energy) Types
 */

export interface BleStatus {
	enabled: boolean;
	running: boolean;
	scanner_active?: boolean;
	metrics?: {
		adv_total: number;
		valid_packets: number;
		parser_errors: number;
		cache_drops: number;
		mutex_timeouts: number;
		scanner_running: boolean;
	};
	devices?: Array<{
		mac: string;
		temp: number;
		humid: number;
		batt: number;
		rssi: number;
		last_seen: number;
		saved?: boolean;
	}>;
}

export interface BleSensorConfig {
	mac: string;
	alias: string;
}

export interface BleSettings {
	enabled: boolean;
	sensors?: BleSensorConfig[];
}
