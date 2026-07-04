import { createSystemChannelSubscription } from './system/channelSubscription.svelte';
import { systemStatus } from './systemStatus.svelte';
import type { SystemEvent } from './system/types';
import type { BleSettings, BleStatus } from '$lib/types/connectivity/ble';

import { normalizeMac } from '$lib/utils/ble';

export interface BleDevice {
	mac: string;
	temp: number;
	humid: number;
	batt: number;
	rssi: number;
	lastSeen: number;
	alias?: string;
	saved?: boolean;
}

interface BleSnapshot {
	running?: boolean;
	scanner_active?: boolean;
	enabled?: boolean;
	settings?: BleSettings;
	devices?: BleStatus['devices'];
}

interface BluetoothStatusStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
	getSnapshot<TSnapshot>(channel: string): TSnapshot | null;
}

interface BluetoothEventBus {
	subscribe(run: (value: SystemEvent | null) => void): () => void;
}

interface BluetoothStoreDeps {
	statusStore?: BluetoothStatusStore;
	eventBus?: BluetoothEventBus;
}

export class BluetoothStore {
	private _devices = $state<Record<string, BleDevice>>({});
	private _status = $state<BleStatus | null>(null);
	private _settings = $state<BleSettings | null>(null);
	private _settingsLoading = $state(true);
	private _settingsError = $state<string | null>(null);

	private _subscriptionCount = 0;
	private _subscription: ReturnType<typeof createSystemChannelSubscription<BleSnapshot>> | null =
		null;
	private readonly statusStore?: BluetoothStatusStore;
	private readonly eventBus?: BluetoothEventBus;

	constructor(deps: BluetoothStoreDeps = {}) {
		this.statusStore = deps.statusStore ?? systemStatus;
		this.eventBus = deps.eventBus;
	}

	start() {
		this._subscriptionCount++;
		if (this._subscriptionCount === 1) {
			this._subscription = createSystemChannelSubscription<BleSnapshot>({
				channel: 'ble',
				onSnapshot: (snapshot) => this.applySnapshot(snapshot),
				onEvent: (event: SystemEvent) => {
					if (event.type === 'ble') {
						this.updateDevice(event.data);
					}
				},
				systemStatusStore: this.statusStore,
				systemEventsBus: this.eventBus
			});
			const hasCached = this._subscription.subscribe();
			if (!hasCached) {
				this._settingsLoading = true;
			}
		}
	}

	stop() {
		if (this._subscriptionCount > 0) {
			this._subscriptionCount--;
			if (this._subscriptionCount === 0 && this._subscription) {
				this._subscription.destroy();
				this._subscription = null;
			}
		}
	}

	get devices() {
		return this._devices;
	}

	get status() {
		return this._status;
	}

	get settings() {
		return this._settings;
	}

	get settingsLoading() {
		return this._settingsLoading;
	}

	get settingsError() {
		return this._settingsError;
	}

	refresh() {
		if (this._subscriptionCount === 0) {
			return;
		}
		this._subscription?.refresh();
	}

	setSettings(settings: BleSettings) {
		this._settings = settings;
		this._settingsError = null;
		this._settingsLoading = false;
		this._status = {
			enabled: settings.enabled,
			running: this._status?.running ?? false,
			scanner_active: this._status?.scanner_active ?? false,
			devices: this._status?.devices
		};
	}

	setStatus(status: BleStatus) {
		this._status = status;
		if (status.devices) {
			this.replaceDevices(
				status.devices.map((device) => ({
					...device,
					lastSeen: device.last_seen,
					saved: device.saved
				}))
			);
		}
	}

	private applySnapshot(snapshot: BleSnapshot) {
		if (snapshot.settings) {
			this.setSettings(snapshot.settings);
		}

		if (
			snapshot.enabled !== undefined ||
			snapshot.running !== undefined ||
			snapshot.scanner_active !== undefined
		) {
			this._status = {
				enabled: snapshot.enabled ?? this._status?.enabled ?? false,
				running: snapshot.running ?? this._status?.running ?? false,
				scanner_active: snapshot.scanner_active ?? this._status?.scanner_active ?? false,
				devices: snapshot.devices ?? this._status?.devices
			};
		}

		if (snapshot.devices) {
			this._status = {
				enabled: this._status?.enabled ?? false,
				running: this._status?.running ?? false,
				scanner_active: this._status?.scanner_active ?? false,
				devices: snapshot.devices
			};
			this.replaceDevices(
				snapshot.devices.map((device) => ({
					...device,
					lastSeen: device.last_seen,
					saved: device.saved
				}))
			);
		}
	}

	updateDevice(device: BleDevice) {
		const mac = normalizeMac(device.mac);
		if (!mac) return;
		const existing = this._devices[mac];
		// Replace the root map so scanner views observing only the top-level
		// devices object still react to incremental BLE websocket updates.
		this._devices = {
			...this._devices,
			[mac]: { ...existing, ...device, mac }
		};
	}

	replaceDevices(devices: BleDevice[]) {
		const nextDevices: Record<string, BleDevice> = {};

		for (const device of devices) {
			const mac = normalizeMac(device.mac);
			if (!mac) continue;
			nextDevices[mac] = { ...this._devices[mac], ...device, mac };
		}

		this._devices = nextDevices;
	}

	reset() {
		this._devices = {};
		this._status = null;
		this._settings = null;
		this._settingsLoading = true;
		this._settingsError = null;
	}
}

export const bluetoothStore = new BluetoothStore();
