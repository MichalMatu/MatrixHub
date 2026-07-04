/**
 * System information types
 */

/**
 * Storage metrics for a single backend (LittleFS, SD card, etc.)
 * Matches SystemStatusResponseAssembler::writeStorageMetrics()
 */
type StorageMetricsInfo = {
	backend: string;
	available: boolean;
	mounted: boolean;
	total_bytes: number;
	used_bytes: number;
	free_bytes: number;
	last_error?: string;
};

/**
 * Complete storage section from system_status snapshot (WS /ws/system)
 * Matches SystemStatusResponseAssembler::writeStorage()
 */
export type SystemStorageInfo = {
	filesystem: { total_bytes: number; used_bytes: number; free_bytes: number };
	active_backend: string;
	active_path: string;
	active: StorageMetricsInfo;
	littlefs: StorageMetricsInfo;
	sdcard: StorageMetricsInfo;
};

export type SystemInformation = {
	esp_platform: string;
	firmware_version: string;
	firmware_name?: string;
	firmware_built_target?: string;
	cpu_freq_mhz: number;
	cpu_type: string;
	cpu_rev: number;
	cpu_cores: number;
	sketch_size: number;
	free_sketch_space: number;
	sdk_version: string;
	arduino_version: string;
	flash_chip_size: number;
	flash_chip_speed: number;
	cpu_reset_reason: number;
	max_alloc_heap: number;
	psram_size: number;
	free_psram: number;
	used_psram: number;
	free_heap: number;
	used_heap: number;
	total_heap: number;
	min_free_heap: number;
	core_temp: number;
	fs_total: number;
	fs_used: number;
	lp_sram_used?: number;
	lp_sram_free?: number;
	lp_sram_total?: number;
	uptime: number;
	mac_address?: string;
	compile_date?: string;
	compile_time?: string;
	storage?: SystemStorageInfo;
};
