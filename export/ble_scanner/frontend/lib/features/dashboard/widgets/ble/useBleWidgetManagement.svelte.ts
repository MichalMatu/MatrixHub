import { untrack } from 'svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { bluetoothStore, type BleDevice } from '$lib/stores/bluetooth.svelte';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import type { BleSettings } from '$lib/types/connectivity/ble';
import { normalizeMac } from '$lib/utils/ble';

export interface BleSensorData {
	mac: string;
	temp: number;
	humid: number;
	batt: number;
	rssi: number;
	lastSeen: number;
	alias?: string;
}

export interface BleSensorDisplay {
	config: { mac: string; alias: string };
	data: BleSensorData | null;
}

interface BleStoreLike {
	readonly devices: Record<string, BleDevice>;
	readonly settings: BleSettings | null;
	readonly settingsLoading: boolean;
	readonly settingsError: string | null;
	start(): void;
	stop(): void;
	replaceDevices(devices: BleDevice[]): void;
}

interface BleWidgetDeps {
	bleStore?: BleStoreLike;
}

export function useBleWidgetManagement(deps: BleWidgetDeps = {}) {
	const session = useSessionAccess();
	const bleStore = deps.bleStore ?? bluetoothStore;

	let now = $state(Date.now());

	const sensorAliases = $derived.by<Record<string, string>>(() => {
		const aliases: Record<string, string> = {};
		for (const sensor of bleStore.settings?.sensors ?? []) {
			const normalizedMac = normalizeMac(sensor.mac);
			if (!normalizedMac) continue;
			aliases[normalizedMac] = sensor.alias;
		}
		return aliases;
	});

	const liveData = $derived.by<Record<string, BleSensorData>>(() => {
		const nextLiveData: Record<string, BleSensorData> = {};

		for (const [mac, device] of Object.entries(bleStore.devices)) {
			const normalizedMac = normalizeMac(mac);
			if (!normalizedMac) continue;

			nextLiveData[normalizedMac] = {
				...device,
				alias: sensorAliases[normalizedMac],
				mac: normalizedMac
			};
		}

		return nextLiveData;
	});

	const sensors = $derived.by<BleSensorDisplay[]>(() => {
		if (!bleStore.settings?.sensors) return [];
		return bleStore.settings.sensors.map((config) => ({
			config,
			data: liveData[normalizeMac(config.mac)] ?? null
		}));
	});

	const scannerEnabled = $derived(bleStore.settings?.enabled ?? false);
	const hasSensors = $derived(sensors.length > 0);
	const widgetError = $derived(bleStore.settingsError);
	const shouldInit = $derived(session.isAuthenticated);

	let tickInterval: ReturnType<typeof setInterval> | undefined;

	const RSSI_STRENGTH_GOOD = -60;
	const RSSI_STRENGTH_FAIR = -75;
	const BATT_LEVEL_OK = 50;
	const BATT_LEVEL_LOW = 20;
	const TICK_INTERVAL_MS = 1000;

	function init() {
		if (!session.isAuthenticated) {
			return;
		}

		startTicker();
		bleStore.start();
	}

	function destroy() {
		if (tickInterval) {
			clearInterval(tickInterval);
			tickInterval = undefined;
		}
		bleStore.stop();
	}

	function startTicker() {
		if (tickInterval) return;
		tickInterval = setInterval(() => {
			now = Date.now();
		}, TICK_INTERVAL_MS);
	}

	$effect(() => {
		if (!shouldInit) return;

		untrack(() => {
			init();
		});

		return () => {
			untrack(() => {
				destroy();
			});
		};
	});

	function getTimeSince(lastSeen: number): string {
		if (lastSeen <= 0) {
			// REST/snapshot data intentionally sends 0 while device wall-clock time is
			// untrusted. Live WS events overwrite this with browser-local Date.now(), so
			// "--" here means "reading exists, but wall-clock age is not trustworthy yet".
			return '--';
		}

		// Clamp small future skew instead of showing negative relative time.
		const seconds = Math.floor(Math.max(0, now - lastSeen) / 1000);

		if (seconds < 60) {
			return m.time_ago_s({ s: seconds }, { locale: i18n.languageTag });
		}

		if (seconds < 3600) {
			const minutes = Math.floor(seconds / 60);
			return m.time_ago_m({ m: minutes, s: seconds % 60 }, { locale: i18n.languageTag });
		}

		const hours = Math.floor(seconds / 3600);
		const minutes = Math.floor((seconds % 3600) / 60);
		return m.time_ago_h({ h: hours, m: minutes }, { locale: i18n.languageTag });
	}

	function getRssiClass(rssi: number): string {
		if (rssi >= RSSI_STRENGTH_GOOD) return 'text-success';
		if (rssi >= RSSI_STRENGTH_FAIR) return 'text-warning';
		return 'text-error';
	}

	function getBatteryClass(batt: number): string {
		if (batt >= BATT_LEVEL_OK) return 'text-success';
		if (batt >= BATT_LEVEL_LOW) return 'text-warning';
		return 'text-error';
	}

	return {
		get settings() {
			return bleStore.settings;
		},
		get liveData() {
			return liveData;
		},
		get now() {
			return now;
		},
		get settingsLoading() {
			return bleStore.settingsLoading;
		},
		get settingsError() {
			return bleStore.settingsError;
		},
		get sensors() {
			return sensors;
		},
		get scannerEnabled() {
			return scannerEnabled;
		},
		get hasSensors() {
			return hasSensors;
		},
		get widgetError() {
			return widgetError;
		},
		init,
		destroy,
		getTimeSince,
		getRssiClass,
		getBatteryClass
	};
}
