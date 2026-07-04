/**
 * Shared types for health diagnostics
 * Extracted from SystemStatus.svelte and HealthDiagnostics.svelte
 */

export interface HealthDiagnostics {
	healthy: boolean;
	issues?: string[];
	heap: {
		free: number;
		min: number;
		largest: number;
		fragmentation: number;
	};
	wifi: {
		connected: boolean;
		rssi?: number;
		reconnects: number;
		ssid?: string;
		ip?: string;
	};
	runtime: {
		uptimeMs: number;
		uptimeHours: number;
		loopCount: number;
		slowLoops: number;
		maintenanceSleeps?: number;
		maintenanceSleepActive?: boolean;
	};
}
