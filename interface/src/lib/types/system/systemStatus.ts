export interface SystemStatus {
	// Sync Details
	timestamp: number; // UTC Epoch from device
	lastUpdate: number; // Local processing time

	// WiFi
	wifiStatus: number; // WL_CONNECTED = 3
	rssi: number;

	// Computed/Derived
	isConnected: boolean;
	isStaConnected?: boolean;
	isApMode?: boolean;
	coreTemp?: number;
}

export const DEFAULT_SYSTEM_STATUS: SystemStatus = {
	timestamp: 0,
	lastUpdate: 0,
	wifiStatus: 0,
	rssi: 0,
	isConnected: false,
	isStaConnected: false,
	isApMode: false,
	coreTemp: 0
};
