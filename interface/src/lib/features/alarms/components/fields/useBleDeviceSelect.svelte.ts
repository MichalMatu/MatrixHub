import { onDestroy, onMount } from 'svelte';
import { bluetoothStore, type BleDevice } from '$lib/stores/bluetooth.svelte';
import type { AlarmSource } from '$lib/types/domain/alarms';
import type { BleSensorConfig } from '$lib/types/connectivity/ble';
import { normalizeMac } from '$lib/utils';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

interface LocalBleDevice extends BleDevice {
	alias?: string;
	saved?: boolean;
}

type BleStoreLike = Pick<typeof bluetoothStore, 'start' | 'stop' | 'refresh'> & {
	readonly devices: Record<string, BleDevice>;
	readonly status: { enabled: boolean; running: boolean } | null;
	readonly settings: { sensors?: BleSensorConfig[] } | null;
	readonly settingsLoading: boolean;
	readonly settingsError: string | null;
};

type BleDeviceSelectDeps = {
	bleStore?: BleStoreLike;
};

export function useBleDeviceSelect(
	getSource: () => AlarmSource | undefined,
	getSelectedMac: () => string | undefined,
	setSelectedMac: (value: string) => void,
	deps: BleDeviceSelectDeps = {}
) {
	const bleStore = deps.bleStore ?? bluetoothStore;

	let liveDevicesMap = $derived(bleStore.devices);
	let savedSensors = $derived(bleStore.settings?.sensors ?? []);
	let loading = $derived(bleStore.settingsLoading);
	let error = $derived(bleStore.settingsError);
	let bleEnabled = $derived(
		bleStore.status === null ? true : Boolean(bleStore.status.enabled && bleStore.status.running)
	);

	onMount(() => {
		bleStore.start();
	});

	onDestroy(() => {
		bleStore.stop();
	});

	let displaySource = $derived(getSource() ?? 'ble_temperature');

	function formatDeviceLabel(device: LocalBleDevice): string {
		const suffix = device.mac.slice(-5);
		const name = device.alias ? device.alias : `Sensor ${suffix}`;
		const reading = (() => {
			if (displaySource === 'ble_humidity') {
				return Number.isFinite(device.humid) ? ` (${device.humid.toFixed(0)}%)` : '';
			}
			if (displaySource === 'ble_battery') {
				return Number.isFinite(device.batt) ? ` (${device.batt.toFixed(0)}%)` : '';
			}
			if (displaySource === 'ble_rssi') {
				return Number.isFinite(device.rssi) ? ` (${device.rssi.toFixed(0)} dBm)` : '';
			}
			return Number.isFinite(device.temp) ? ` (${device.temp.toFixed(1)}°C)` : '';
		})();
		const saved = device.saved ? ' ★' : '';
		return `${name}${reading}${saved}`;
	}

	function refresh() {
		bleStore.refresh();
	}

	$effect(() => {
		if ((getSelectedMac() ?? '') === '' && savedSensors.length === 1) {
			setSelectedMac(normalizeMac(savedSensors[0].mac));
		}
	});

	let devices = $derived.by(() => {
		return savedSensors
			.map((sensor) => {
				const normalizedMac = normalizeMac(sensor.mac);
				const liveDevice = liveDevicesMap[normalizedMac];

				return {
					mac: normalizedMac,
					temp: liveDevice?.temp ?? Number.NaN,
					humid: liveDevice?.humid ?? Number.NaN,
					batt: liveDevice?.batt ?? Number.NaN,
					rssi: liveDevice?.rssi ?? -100,
					lastSeen: liveDevice?.lastSeen ?? 0,
					alias: sensor.alias,
					saved: true
				} satisfies LocalBleDevice;
			})
			.sort((left, right) => right.rssi - left.rssi);
	});

	let options = $derived.by(() => [
		{ value: '', label: m.alarms_select_ble_device({ locale: i18n.languageTag }) },
		...devices.map((device) => ({
			value: device.mac,
			label: formatDeviceLabel(device)
		}))
	]);

	let isValidSelection = $derived.by(() => {
		const selected = normalizeMac(getSelectedMac() ?? '');
		return selected === '' || devices.some((device) => device.mac === selected);
	});

	return {
		get loading() {
			return loading;
		},
		get error() {
			return error;
		},
		get bleEnabled() {
			return bleEnabled;
		},
		get options() {
			return options;
		},
		get isValidSelection() {
			return isValidSelection;
		},
		refresh
	};
}
