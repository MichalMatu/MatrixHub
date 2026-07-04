/**
 * WiFi Sensing types shared between components
 */

export interface WifiSensingStats {
	current: number;
	filtered?: number;
	min: number;
	max: number;
	avg: number;
	variance: number;
	sampleCount: number;
	windowMs: number;
}

export interface WifiSensingData {
	enabled: boolean;
	running: boolean;
	active: boolean;
	connectedSSID: string;
	connectedChannel: number;
	stats: WifiSensingStats;
	motionDetected: boolean;
	variance_threshold: number;
	samples?: RssiSample[];
}

export interface WifiSensingSettings {
	enabled: boolean;
	sample_interval_ms: number;
	variance_threshold: number;
	csi_alarm: CsiAlarmSettings;
}

export interface CsiAlarmBand {
	start: number;
	end: number;
}

export interface CsiAlarmSettings {
	enabled: boolean;
	bands: CsiAlarmBand[];
	baseline_frames: number;
	top_k: number;
	enter_threshold: number;
	clear_threshold: number;
	hold_ms: number;
	clear_hold_ms: number;
	min_noise: number;
	min_energy: number;
	noisy_threshold: number;
	auto_recalibration: boolean;
	sensitivity: 0 | 1 | 2;
}

export interface CsiMotionStatus {
	enabled: boolean;
	state: string;
	baseline_ready: boolean;
	detected: boolean;
	noisy: boolean;
	needs_calibration: boolean;
	score: number;
	confidence: number;
	frames_seen: number;
	width: number;
	band_count: number;
	selected_carriers: number;
	valid_carriers: number;
	last_reset_reason: string;
}

export interface CsiRuntimeMetrics {
	enabled: boolean;
	queue_allocated: boolean;
	active_consumer_mask: number;
	consumer_count: number;
	frontend_consumer_active: boolean;
	alarm_consumer_active: boolean;
	boot_consumer_active: boolean;
	matrix_visualization_consumer_active: boolean;
	queue_depth: number;
	queue_capacity: number;
	queue_drops_total: number;
	queue_drops_last_sec: number;
	rx_frames_total: number;
	rx_accepted_total: number;
	rx_throttled_total: number;
	queued_packets_total: number;
	dequeued_packets_total: number;
	packets_forwarded_total: number;
	batches_forwarded_total: number;
	batches_dropped_total: number;
	packets_per_sec: number;
	batches_per_sec: number;
	last_packet_ms: number;
	last_batch_ms: number;
	calibration_count: number;
	calibration_target: number;
	calibration_state: string;
	motion: CsiMotionStatus;
	ws_client_count: number;
	ws_queue_enabled: boolean;
}

export interface WifiSensingStatus extends WifiSensingData {
	schema: 'wifisensing.status.v1';
	sample_interval_ms: number;
	csi: CsiRuntimeMetrics;
}

export interface RssiSample {
	rssi: number;
	timestamp: number;
	variance?: number;
}
