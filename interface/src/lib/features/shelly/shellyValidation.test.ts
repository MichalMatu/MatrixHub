import { describe, expect, it } from 'vitest';
import { sanitizeShellyIpv4Input, validateShellyDraft } from './shellyValidation';

describe('shellyValidation', () => {
	it('sanitizes IPv4-like user input down to the host part', () => {
		expect(sanitizeShellyIpv4Input('https://192.168.0.20:80/relay/0')).toBe('192.168.0.20');
		expect(sanitizeShellyIpv4Input('  http://10.0.0.4/  ')).toBe('10.0.0.4');
	});

	it('accepts valid IPv4 drafts and rejects invalid addresses', () => {
		const valid = validateShellyDraft({
			id: '',
			name: 'Desk',
			ip: 'http://192.168.1.20/',
			relay_index: 0,
			generation: 2
		});
		const invalid = validateShellyDraft({
			id: '',
			name: 'Desk',
			ip: 'https://shelly.local/relay/0',
			relay_index: 0,
			generation: 2
		});

		expect(valid.valid).toBe(true);
		expect(valid.normalizedDraft.ip).toBe('192.168.1.20');
		expect(valid.errors.ip).toBe(false);
		expect(invalid.valid).toBe(false);
		expect(invalid.errors.ip).toBe(true);
		expect(invalid.normalizedDraft.ip).toBe('shelly.local');
	});
});
