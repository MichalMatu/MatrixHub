import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';

export interface ShellyDeviceDraft {
	id: string;
	name: string;
	ip: string;
	relay_index: number;
	generation: number;
}

export const MAX_SHELLY_DEVICES = 4;
export const DEFAULT_SHELLY_GENERATION = 2;
const DEFAULT_SHELLY_RELAY_INDEX = 0;
const SHELLY_GENERATION_VALUES = new Set([1, 2]);

export function createShellyDeviceDraft(defaultName: string): ShellyDeviceDraft {
	return {
		id: '',
		name: defaultName,
		ip: '',
		relay_index: DEFAULT_SHELLY_RELAY_INDEX,
		generation: DEFAULT_SHELLY_GENERATION
	};
}

export function createShellyDeviceDraftFromDevice(device: ShellyDevice): ShellyDeviceDraft {
	return {
		id: device.id,
		name: device.name,
		ip: device.ip,
		relay_index: normalizeShellyRelayIndex(device.relay_index),
		generation: normalizeShellyGeneration(device.generation ?? DEFAULT_SHELLY_GENERATION)
	};
}

export function generateShellyDeviceId(): string {
	return Math.random().toString(36).slice(2, 10);
}

export function normalizeShellyRelayIndex(
	value: number,
	fallback: number = DEFAULT_SHELLY_RELAY_INDEX
): number {
	if (!Number.isFinite(value)) return fallback;
	return Math.max(0, Math.trunc(value));
}

export function normalizeShellyGeneration(
	value: number,
	fallback: number = DEFAULT_SHELLY_GENERATION
): number {
	return SHELLY_GENERATION_VALUES.has(value) ? value : fallback;
}

export function createShellyPendingDevice(
	draft: ShellyDeviceDraft,
	options: { enabled?: boolean } = {}
): ShellyDevice {
	return {
		...draft,
		enabled: options.enabled ?? true
	};
}

export function upsertShellyDevice(devices: ShellyDevice[], device: ShellyDevice): ShellyDevice[] {
	const index = devices.findIndex((current) => current.id === device.id);
	if (index === -1) {
		return [...devices, device];
	}

	const nextDevices = [...devices];
	nextDevices[index] = device;
	return nextDevices;
}

export function applyShellyRealtimeUpdate(
	devices: ShellyDevice[],
	update: Partial<ShellyDevice> & Pick<ShellyDevice, 'id'>
): ShellyDevice[] {
	const index = devices.findIndex((device) => device.id === update.id);
	if (index === -1) return devices;

	const current = devices[index];
	const nextDevices = [...devices];
	nextDevices[index] = {
		...current,
		isOn: update.isOn ?? current.isOn,
		isOnline: update.isOnline ?? current.isOnline,
		power: update.power ?? current.power,
		voltage: update.voltage ?? current.voltage,
		current: update.current ?? current.current,
		energy: update.energy ?? current.energy,
		temperature: update.temperature ?? current.temperature,
		rssi: update.rssi ?? current.rssi
	};

	return nextDevices;
}
