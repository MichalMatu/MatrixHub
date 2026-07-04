import { describe, expect, it } from 'vitest';
import {
	WifiNetworkSchema,
	createWifiApFormErrors,
	createWifiStaSettingsErrors,
	validateWifiApSettings,
	validateWifiStaSettings
} from './wifiValidation';

describe('wifiValidation', () => {
	it('accepts SSIDs up to 32 UTF-8 bytes', () => {
		const asciiResult = WifiNetworkSchema.safeParse({
			ssid: '12345678901234567890123456789012',
			password: ''
		});
		const multibyteResult = WifiNetworkSchema.safeParse({
			ssid: 'ą'.repeat(16),
			password: ''
		});

		expect(asciiResult.success).toBe(true);
		expect(multibyteResult.success).toBe(true);
	});

	it('rejects SSIDs that exceed 32 UTF-8 bytes even when character count looks valid', () => {
		const result = WifiNetworkSchema.safeParse({
			ssid: 'ą'.repeat(17),
			password: ''
		});

		expect(result.success).toBe(false);
		if (result.success) return;

		expect(result.error.flatten().fieldErrors.ssid).toContain('SSID too long (max 32 bytes)');
	});

	it('reports per-field static IPv4 errors for saved networks', () => {
		const result = WifiNetworkSchema.safeParse({
			ssid: 'Home',
			password: 'secret123',
			static_ip_config: true,
			local_ip: '999.999.999.999',
			subnet_mask: '',
			gateway_ip: '192.168.1.999',
			dns_ip_1: '8.8.8.999',
			dns_ip_2: ''
		});

		expect(result.success).toBe(false);
		if (result.success) return;

		const fieldErrors = result.error.flatten().fieldErrors;
		expect(fieldErrors.local_ip).toContain('Local IP must be a valid IPv4 address');
		expect(fieldErrors.subnet_mask).toContain('Subnet mask is required');
		expect(fieldErrors.gateway_ip).toContain('Gateway IP must be a valid IPv4 address');
		expect(fieldErrors.dns_ip_1).toContain('DNS 1 must be a valid IPv4 address');
	});

	it('validates AP settings through a shared source of truth', () => {
		const valid = validateWifiApSettings({
			ssid: 'A',
			password: 'secret123',
			channel: 6,
			ssid_hidden: false,
			max_clients: 4,
			local_ip: '192.168.4.1',
			gateway_ip: '192.168.4.1',
			subnet_mask: '255.255.255.0'
		});
		const invalid = validateWifiApSettings({
			ssid: '',
			password: '1234',
			channel: 99,
			ssid_hidden: true,
			max_clients: 0,
			local_ip: 'invalid',
			gateway_ip: 'also-invalid',
			subnet_mask: 'bad-mask'
		});

		expect(valid).toEqual({ valid: true, errors: createWifiApFormErrors() });
		expect(invalid.valid).toBe(false);
		expect(invalid.errors).toEqual({
			ssid: true,
			password: true,
			channel: true,
			max_clients: true,
			local_ip: true,
			gateway_ip: true,
			subnet_mask: true
		});
	});

	it('blocks Wi-Fi STA settings when one saved network is invalid', () => {
		const result = validateWifiStaSettings({
			hostname: 'esp32-node',
			mode: 'sta',
			wifi_networks: [
				{
					ssid: 'Home',
					password: 'secret123',
					static_ip_config: true,
					local_ip: '192.168.1.10',
					subnet_mask: '255.255.255.0',
					gateway_ip: '192.168.1.1',
					dns_ip_1: '8.8.8.8',
					dns_ip_2: '999.999.999.999'
				}
			]
		});

		expect(result.valid).toBe(false);
		expect(result.errors).toEqual({
			hostname: false,
			wifi_networks: true
		});
		expect(createWifiStaSettingsErrors()).toEqual({
			hostname: false,
			wifi_networks: false
		});
	});

	it('requires saved networks only in station mode', () => {
		expect(
			validateWifiStaSettings({
				hostname: 'esp32-node',
				mode: 'ap',
				wifi_networks: []
			}).valid
		).toBe(true);
		expect(
			validateWifiStaSettings({
				hostname: 'esp32-node',
				mode: 'off',
				wifi_networks: []
			}).valid
		).toBe(true);

		const sta = validateWifiStaSettings({
			hostname: 'esp32-node',
			mode: 'sta',
			wifi_networks: []
		});

		expect(sta.valid).toBe(false);
		expect(sta.errors.wifi_networks).toBe(true);
	});
});
