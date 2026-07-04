import { describe, expect, it } from 'vitest';

import { parseBleStatusSnapshot } from '@matrixhub/device-sdk';

describe('device SDK BLE parser', () => {
	it('preserves BLE runtime metrics from status snapshots', () => {
		const parsed = parseBleStatusSnapshot({
			enabled: true,
			running: true,
			scanner_active: true,
			metrics: {
				adv_total: 42,
				valid_packets: 7,
				parser_errors: 3,
				cache_drops: 1,
				mutex_timeouts: 2,
				scanner_running: true
			}
		});

		expect(parsed?.metrics).toEqual({
			adv_total: 42,
			valid_packets: 7,
			parser_errors: 3,
			cache_drops: 1,
			mutex_timeouts: 2,
			scanner_running: true
		});
	});

	it('drops malformed BLE runtime metrics without rejecting the status snapshot', () => {
		const parsed = parseBleStatusSnapshot({
			enabled: true,
			running: false,
			metrics: {
				adv_total: 42,
				valid_packets: 7,
				parser_errors: 'bad',
				cache_drops: 1,
				mutex_timeouts: 2,
				scanner_running: true
			}
		});

		expect(parsed).toMatchObject({ enabled: true, running: false });
		expect(parsed?.metrics).toBeUndefined();
	});
});
