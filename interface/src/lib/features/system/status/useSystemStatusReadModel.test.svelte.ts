import { describe, expect, it } from 'vitest';
import { useSystemStatusReadModel } from './useSystemStatusReadModel.svelte';

describe('useSystemStatusReadModel', () => {
	it('exposes status, cpu temperature, and station connectivity through a read model', () => {
		const store = {
			data: {
				timestamp: 0,
				lastUpdate: 0,
				wifiStatus: 3,
				rssi: -55,
				isConnected: true,
				isStaConnected: false,
				isApMode: false,
				coreTemp: 47.5
			}
		};

		const readModel = useSystemStatusReadModel({ store });

		expect(readModel.status.rssi).toBe(-55);
		expect(readModel.coreTemp).toBe(47.5);
		expect(readModel.isStaConnected).toBe(false);
	});
});
