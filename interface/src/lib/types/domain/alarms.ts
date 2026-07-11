/**
 * Alarm types shared between components and services
 */

/** Signal source for alarm condition */
const DEFAULT_COOLDOWN_SECONDS = 600;
export const MAX_ALARM_NAME_LENGTH = 63; // 64 bytes in firmware
export const MAX_ALARM_RULES = 8; // matches firmware kMaxRules / kMaxAlarmRules

export type AlarmSource =
	| 'co2'
	| 'temperature'
	| 'humidity'
	| 'wifi_motion'
	| 'ble_temperature'
	| 'ble_humidity'
	| 'ble_battery'
	| 'ble_rssi'
	| 'wifi_csi_motion'
	| 'imu_tamper'
	| 'gpio_digital';

/** Comparison operator */
export type AlarmOperator = 'above' | 'below';

/** Notification channel */
export type NotifyChannel = 'telegram' | 'led' | 'webhook' | 'pushover';

/** Severity level */
export type AlarmSeverity = 'info' | 'warning' | 'critical';

/** Single alarm rule */
export interface AlarmRule {
	id: string;
	enabled: boolean;
	name: string;
	source: AlarmSource;
	operator: AlarmOperator;
	threshold: number;
	severity: AlarmSeverity;
	notify_channels: NotifyChannel[];
	cooldown_seconds: number;
	shelly_device_ids?: string[];
	ble_device_mac?: string; // BLE device MAC for BLE-backed sources
	gpio_id?: string;
	created_at?: number;
	updated_at?: number;
	// Runtime state (mixed in from status endpoint)
	triggered?: boolean;
	last_triggered?: number;
	current_value?: number; // Current sensor value for this rule's source
	transition_seq?: number; // Per-rule, boot-scoped transition sequence
	device_millis?: number; // Device millis() captured for the last transition
	boot_id?: string; // Exact 16-character lowercase hexadecimal boot epoch
}

/** Configuration containing all alarm rules */
export interface AlarmRulesConfig {
	schema_version: 1;
	rules: AlarmRule[];
}

/** Display metadata for signal sources */
export const ALARM_SOURCES: Record<
	AlarmSource,
	{ unit: string; icon: string; booleanLike?: boolean }
> = {
	co2: { unit: 'ppm', icon: 'cloud' },
	temperature: { unit: '°C', icon: 'thermometer' },
	humidity: { unit: '%', icon: 'droplet' },
	wifi_motion: { unit: 'v', icon: 'wifi' },
	ble_temperature: { unit: '°C', icon: 'bluetooth' },
	ble_humidity: { unit: '%', icon: 'bluetooth' },
	ble_battery: { unit: '%', icon: 'bluetooth' },
	ble_rssi: { unit: 'dBm', icon: 'bluetooth' },
	wifi_csi_motion: { unit: '', icon: 'wifi', booleanLike: true },
	imu_tamper: { unit: '', icon: 'activity', booleanLike: true },
	gpio_digital: { unit: '', icon: 'cpu', booleanLike: true }
};

/** Display metadata for severity levels */
export const SEVERITY_CONFIG: Record<AlarmSeverity, { color: string; badgeClass: string }> = {
	info: { color: 'blue', badgeClass: 'badge-info' },
	warning: { color: 'yellow', badgeClass: 'badge-warning' },
	critical: { color: 'red', badgeClass: 'badge-error' }
};

/** Default values for new alarm rule */
export const DEFAULT_ALARM_RULE: Omit<AlarmRule, 'id' | 'created_at' | 'updated_at'> = {
	enabled: false,
	name: '',
	source: 'temperature',
	operator: 'above',
	threshold: 30,
	severity: 'warning',
	// Keep the frontend "new rule" draft local-first by default.
	// Firmware now uses the same default, so if this ever drifts again, check
	// src/alarms/types/AlarmRule.h and the alarm form auto-name tests together.
	notify_channels: ['led'],
	cooldown_seconds: DEFAULT_COOLDOWN_SECONDS,
	shelly_device_ids: [],
	gpio_id: ''
};

/** Generate unique ID for new rule */
export function generateId(): string {
	// Firmware limit: kMaxIdLen=32 -> max string length 31.
	// Keep IDs short and URL/JSON-friendly.
	const ts = Date.now().toString(36); // ~8 chars
	const rnd = Math.random().toString(36).slice(2, 6); // 4 chars
	return `a_${ts}_${rnd}`.slice(0, 31);
}
