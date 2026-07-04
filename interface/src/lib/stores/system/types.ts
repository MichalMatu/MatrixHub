import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';

export type ShellyEventData = {
	id: string;
	online: boolean;
	on: boolean;
	power: number;
	voltage: number;
	current: number;
	energy: number;
	temperature: number;
	rssi: number;
};

export type SensingEventData = {
	timestamp: number;
	rssi: number;
	variance: number;
	motion: boolean;
};

export type AlarmEventData = {
	id: string;
	triggered: boolean;
	current_value: number;
	severity: number;
};

export type TelegramEventData = {
	enabled: boolean;
	running: boolean;
	lastPollAgeSec: number;
	messagesProcessed: number;
	messagesSent: number;
	commandsExecuted: number;
	lastHttpCode: number;
};

export type BleEventData = {
	mac: string;
	temp: number;
	humid: number;
	batt: number;
	rssi: number;
	lastSeen: number;
	alias?: string;
};

export type NotifStatsEventData = {
	webhook: { sent: number; failed: number; lastMs: number; httpCode: number };
	pushover: { sent: number; failed: number; lastMs: number; httpCode: number };
	udp: { sent: number; failed: number; lastMs: number };
	heartbeat: Array<{ lastPingMs: number; successCount: number; failCount: number }>;
	telegram: {
		enabled: boolean;
		running: boolean;
		lastActivityMs: number;
		messagesProcessed: number;
		messagesSent: number;
		commandsExecuted: number;
		lastHttpCode: number;
	};
	uptimeMs: number;
};

export type AirMouseTelemetryEventData = {
	gx: number;
	gy: number;
	gz: number;
	ax: number;
	ay: number;
	az: number;
	deltaG: number;
};

export type SnapshotChannel =
	| 'shelly'
	| 'alarms'
	| 'ble'
	| 'sensing'
	| 'telemetry'
	| 'system_status'
	| 'airmouse'
	| 'telegram'
	| 'notif_stats';

export type SystemEvent =
	| { type: 'event'; source: string; data: unknown }
	| { type: 'macros'; data: ScriptStatus[] }
	| {
			type: 'sensor';
			data: { co2: number; temp: number; humid: number; lastReadOk: boolean; timestamp_ms: number };
	  }
	| { type: 'shelly'; data: ShellyEventData }
	| { type: 'sensing'; data: SensingEventData }
	| { type: 'ble'; data: BleEventData }
	| { type: 'alarm'; data: AlarmEventData }
	| { type: 'telegram'; data: TelegramEventData }
	| { type: 'notif_stats'; data: NotifStatsEventData }
	| { type: 'airmouse'; data: AirMouseTelemetryEventData }
	| { type: 'snapshot'; channel: SnapshotChannel; data: unknown };
