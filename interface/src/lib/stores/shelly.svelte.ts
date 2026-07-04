import { createSystemChannelSubscription } from './system/channelSubscription.svelte';
import type { SystemEvent } from './system/types';
import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';
import { applyShellyRealtimeUpdate, upsertShellyDevice } from '$lib/features/shelly/shellyModel';

interface ShellySnapshotStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
	getSnapshot<TSnapshot>(channel: string): TSnapshot | null;
	requestSnapshot?(channel: string): void;
	subscribeEvents?(run: (value: SystemEvent | null) => void): () => void;
}

interface ShellyEventsBus {
	subscribe(run: (value: SystemEvent | null) => void): () => void;
}

interface ShellyStoreDeps {
	statusStore?: ShellySnapshotStore;
	eventBus?: ShellyEventsBus;
}

export class ShellyStore {
	private _devices = $state<ShellyDevice[]>([]);
	private _loading = $state(true);
	private _errorMessage = $state<string | null>(null);
	private _subscriptionCount = 0;
	private _subscription: ReturnType<typeof createSystemChannelSubscription<ShellyDevice[]>> | null =
		null;
	private readonly statusStore?: ShellySnapshotStore;
	private readonly eventBus?: ShellyEventsBus;

	constructor(deps: ShellyStoreDeps = {}) {
		this.statusStore = deps.statusStore;
		this.eventBus = deps.eventBus;
	}

	start() {
		this._subscriptionCount++;
		if (this._subscriptionCount === 1) {
			this._subscription = createSystemChannelSubscription<ShellyDevice[]>({
				channel: 'shelly',
				onSnapshot: (snapshot) => this.applySnapshot(snapshot),
				onEvent: (event) => this.applyEvent(event),
				systemStatusStore: this.statusStore,
				systemEventsBus: this.eventBus
			});
			return this._subscription.subscribe();
		}
		return this._devices.length > 0;
	}

	stop() {
		if (this._subscriptionCount > 0) {
			this._subscriptionCount--;
		}

		if (this._subscriptionCount === 0 && this._subscription) {
			this.reset();
		}
	}

	get devices() {
		return this._devices;
	}

	get loading() {
		return this._loading;
	}

	get errorMessage() {
		return this._errorMessage;
	}

	hydrateDevices(devices: ShellyDevice[]) {
		this._devices = devices ?? [];
		this._loading = false;
	}

	refresh() {
		this._subscription?.refresh();
	}

	applySnapshot(devices: ShellyDevice[]) {
		this._devices = devices ?? [];
		this._loading = false;
		this._errorMessage = null;
	}

	applyEvent(event: SystemEvent | null) {
		if (event?.type !== 'shelly') return;

		this._devices = applyShellyRealtimeUpdate(this._devices, {
			id: event.data.id,
			isOn: event.data.on,
			isOnline: event.data.online,
			power: event.data.power,
			voltage: event.data.voltage,
			current: event.data.current,
			energy: event.data.energy,
			temperature: event.data.temperature,
			rssi: event.data.rssi
		});
	}

	upsertDevice(device: ShellyDevice) {
		this._devices = upsertShellyDevice(this._devices, device);
		this._errorMessage = null;
	}

	removeDevice(id: string) {
		this._devices = this._devices.filter((device) => device.id !== id);
		this._errorMessage = null;
	}

	updateToggle(id: string, isOn: boolean) {
		const index = this._devices.findIndex((device) => device.id === id);
		if (index === -1) return;
		this._devices[index].isOn = isOn;
	}

	setLoading(value: boolean) {
		this._loading = value;
	}

	setError(message: string | null) {
		this._errorMessage = message;
		if (message) {
			this._loading = false;
		}
	}

	reset() {
		this._subscription?.destroy();
		this._subscription = null;
		this._subscriptionCount = 0;
		this._devices = [];
		this._loading = true;
		this._errorMessage = null;
	}
}

export const shellyStore = new ShellyStore();
