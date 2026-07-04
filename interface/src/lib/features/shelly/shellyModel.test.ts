import { describe, expect, it } from 'vitest';
import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';
import {
	DEFAULT_SHELLY_GENERATION,
	MAX_SHELLY_DEVICES,
	applyShellyRealtimeUpdate,
	createShellyDeviceDraft,
	createShellyDeviceDraftFromDevice,
	createShellyPendingDevice,
	generateShellyDeviceId,
	normalizeShellyGeneration,
	normalizeShellyRelayIndex,
	upsertShellyDevice
} from './shellyModel';

describe('shellyModel', () => {
	it('creates a fresh device draft with repo defaults', () => {
		expect(createShellyDeviceDraft('My Shelly')).toEqual({
			id: '',
			name: 'My Shelly',
			ip: '',
			relay_index: 0,
			generation: DEFAULT_SHELLY_GENERATION
		});
		expect(MAX_SHELLY_DEVICES).toBe(4);
	});

	it('creates an editable draft from an existing device', () => {
		expect(
			createShellyDeviceDraftFromDevice({
				id: 'desk-1',
				name: 'Desk',
				ip: '192.168.1.20',
				relay_index: 3,
				generation: 1
			} as ShellyDevice)
		).toEqual({
			id: 'desk-1',
			name: 'Desk',
			ip: '192.168.1.20',
			relay_index: 3,
			generation: 1
		});
	});

	it('normalizes relay and generation values', () => {
		expect(normalizeShellyRelayIndex(Number.NaN)).toBe(0);
		expect(normalizeShellyRelayIndex(-5)).toBe(0);
		expect(normalizeShellyRelayIndex(3.9)).toBe(3);
		expect(normalizeShellyGeneration(1)).toBe(1);
		expect(normalizeShellyGeneration(99)).toBe(DEFAULT_SHELLY_GENERATION);
	});

	it('applies realtime updates immutably and ignores missing devices', () => {
		const devices: ShellyDevice[] = [
			{ id: 'a', name: 'Desk', ip: '1.1.1.1', relay_index: 0, power: 10, isOn: false }
		];

		expect(applyShellyRealtimeUpdate(devices, { id: 'a', isOn: true, temperature: 22.5 })).toEqual([
			{
				id: 'a',
				name: 'Desk',
				ip: '1.1.1.1',
				relay_index: 0,
				power: 10,
				isOn: true,
				temperature: 22.5
			}
		]);
		expect(applyShellyRealtimeUpdate(devices, { id: 'missing', isOn: true })).toBe(devices);
	});

	it('upserts optimistic devices and creates enabled pending entries', () => {
		const draft = {
			id: 'abc12345',
			name: 'Desk',
			ip: '192.168.1.10',
			relay_index: 0,
			generation: 2
		};
		const pending = createShellyPendingDevice(draft);
		const disabledPending = createShellyPendingDevice(draft, { enabled: false });

		expect(pending.enabled).toBe(true);
		expect(disabledPending.enabled).toBe(false);
		expect(upsertShellyDevice([], pending)).toEqual([pending]);
		expect(upsertShellyDevice([pending], { ...pending, isOnline: true })[0].isOnline).toBe(true);
		expect(generateShellyDeviceId()).toHaveLength(8);
	});
});
