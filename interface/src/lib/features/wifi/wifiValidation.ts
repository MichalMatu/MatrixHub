import { z } from 'zod';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { ApSettings } from '$lib/types/connectivity/ap';
import type { KnownNetworkItem, WifiSettings } from '$lib/types/connectivity/wifi';
import {
	MAX_WIFI_SSID_BYTES,
	isValidIPv4,
	isValidSSID,
	isValidWifiPassword
} from '$lib/utils/validation/validators';

const WIFI_MIN_CHANNEL = 1;
const WIFI_MAX_CHANNEL = 13;
const WIFI_MIN_AP_CLIENTS = 1;
const WIFI_MAX_AP_CLIENTS = 8;
const WIFI_MAX_SAVED_NETWORKS = 5;

function localeOptions() {
	return { locale: i18n.languageTag };
}

function requiredFieldMessage(field: string) {
	return m.wifi_validation_required({ field }, localeOptions());
}

function invalidIpv4Message(field: string) {
	return m.wifi_validation_ipv4({ field }, localeOptions());
}

function wifiFieldLabel(
	field: 'local_ip' | 'subnet_mask' | 'gateway_ip' | 'dns_ip_1' | 'dns_ip_2'
) {
	switch (field) {
		case 'local_ip':
			return m.wifi_field_local_ip(localeOptions());
		case 'subnet_mask':
			return m.wifi_field_subnet_mask(localeOptions());
		case 'gateway_ip':
			return m.wifi_field_gateway_ip(localeOptions());
		case 'dns_ip_1':
			return m.wifi_field_dns_1(localeOptions());
		case 'dns_ip_2':
			return m.wifi_field_dns_2(localeOptions());
	}
}

function addSsidIssues(ssid: string, ctx: z.RefinementCtx) {
	if (ssid.length === 0) {
		ctx.addIssue({
			code: z.ZodIssueCode.custom,
			message: m.wifi_ssid_required(localeOptions())
		});
		return;
	}

	if (!isValidSSID(ssid)) {
		ctx.addIssue({
			code: z.ZodIssueCode.custom,
			message: m.wifi_ssid_too_long({ max: MAX_WIFI_SSID_BYTES }, localeOptions())
		});
	}
}

export const WifiHostnameSchema = z.string().superRefine((hostname, ctx) => {
	if (hostname.length < 3) {
		ctx.addIssue({
			code: z.ZodIssueCode.custom,
			message: m.wifi_hostname_error_min({ min: 3 }, localeOptions())
		});
	}

	if (hostname.length > 32) {
		ctx.addIssue({
			code: z.ZodIssueCode.custom,
			message: m.wifi_hostname_error_max({ max: 32 }, localeOptions())
		});
	}

	if (!/^[a-zA-Z0-9-]+$/.test(hostname)) {
		ctx.addIssue({
			code: z.ZodIssueCode.custom,
			message: m.wifi_hostname_error_format(localeOptions())
		});
	}
});

export interface WifiApFormErrors {
	ssid: boolean;
	password: boolean;
	channel: boolean;
	max_clients: boolean;
	local_ip: boolean;
	gateway_ip: boolean;
	subnet_mask: boolean;
}

export interface WifiStaSettingsErrors {
	hostname: boolean;
	wifi_networks: boolean;
}

export function createWifiApFormErrors(): WifiApFormErrors {
	return {
		ssid: false,
		password: false,
		channel: false,
		max_clients: false,
		local_ip: false,
		gateway_ip: false,
		subnet_mask: false
	};
}

export function createWifiStaSettingsErrors(): WifiStaSettingsErrors {
	return {
		hostname: false,
		wifi_networks: false
	};
}

function requiredIpv4Field(getLabel: () => string) {
	return z
		.string()
		.trim()
		.min(1, requiredFieldMessage(getLabel()))
		.refine((value) => isValidIPv4(value), invalidIpv4Message(getLabel()));
}

const WifiBaseNetworkSchema = z.object({
	ssid: z.string().superRefine(addSsidIssues),
	password: z.string().refine((value) => isValidWifiPassword(value), {
		message: m.wifi_password_error(localeOptions())
	}),
	bssid: z.string().optional()
});

export const WifiNetworkSchema = WifiBaseNetworkSchema.extend({
	static_ip_config: z.boolean().default(false),
	local_ip: z.string().default(''),
	subnet_mask: z.string().default(''),
	gateway_ip: z.string().default(''),
	dns_ip_1: z.string().default(''),
	dns_ip_2: z.string().default('')
}).superRefine((network, ctx) => {
	if (!network.static_ip_config) {
		return;
	}

	const requiredFields = ['local_ip', 'subnet_mask', 'gateway_ip'] as const;

	for (const field of requiredFields) {
		const label = wifiFieldLabel(field);
		const value = network[field]?.trim() ?? '';
		if (!value.length) {
			ctx.addIssue({
				code: z.ZodIssueCode.custom,
				path: [field],
				message: requiredFieldMessage(label)
			});
			continue;
		}

		if (!isValidIPv4(value)) {
			ctx.addIssue({
				code: z.ZodIssueCode.custom,
				path: [field],
				message: invalidIpv4Message(label)
			});
		}
	}

	const optionalFields = ['dns_ip_1', 'dns_ip_2'] as const;

	for (const field of optionalFields) {
		const label = wifiFieldLabel(field);
		const value = network[field]?.trim() ?? '';
		if (value.length && !isValidIPv4(value)) {
			ctx.addIssue({
				code: z.ZodIssueCode.custom,
				path: [field],
				message: invalidIpv4Message(label)
			});
		}
	}
});

const WifiStaSettingsSchema = z
	.object({
		hostname: WifiHostnameSchema,
		mode: z.enum(['off', 'ap', 'sta']),
		wifi_networks: z.array(WifiNetworkSchema)
	})
	.superRefine((settings, ctx) => {
		if (settings.wifi_networks.length > WIFI_MAX_SAVED_NETWORKS) {
			ctx.addIssue({
				code: z.ZodIssueCode.custom,
				path: ['wifi_networks'],
				message: m.wifi_networks_limit_error({ max: WIFI_MAX_SAVED_NETWORKS }, localeOptions())
			});
		}

		if (settings.mode === 'sta' && settings.wifi_networks.length === 0) {
			ctx.addIssue({
				code: z.ZodIssueCode.custom,
				path: ['wifi_networks'],
				message: m.wifi_sta_requires_network(localeOptions())
			});
		}
	});

const WifiApSettingsSchema = z.object({
	ssid: z.string().superRefine(addSsidIssues),
	password: z.string().refine((value) => isValidWifiPassword(value), {
		message: m.wifi_password_error(localeOptions())
	}),
	channel: z.number().min(WIFI_MIN_CHANNEL).max(WIFI_MAX_CHANNEL),
	ssid_hidden: z.boolean(),
	max_clients: z.number().min(WIFI_MIN_AP_CLIENTS).max(WIFI_MAX_AP_CLIENTS),
	local_ip: requiredIpv4Field(() => m.wifi_field_local_ip(localeOptions())),
	gateway_ip: requiredIpv4Field(() => m.wifi_field_gateway_ip(localeOptions())),
	subnet_mask: requiredIpv4Field(() => m.wifi_field_subnet_mask(localeOptions()))
});

function hasIssueForPath(path: ReadonlyArray<PropertyKey>, field: string): boolean {
	return path[0] === field;
}

export function validateWifiApSettings(settings: ApSettings): {
	valid: boolean;
	errors: WifiApFormErrors;
} {
	const errors = createWifiApFormErrors();
	const result = WifiApSettingsSchema.safeParse(settings);

	if (result.success) {
		return { valid: true, errors };
	}

	for (const issue of result.error.issues) {
		if (hasIssueForPath(issue.path, 'ssid')) errors.ssid = true;
		if (hasIssueForPath(issue.path, 'password')) errors.password = true;
		if (hasIssueForPath(issue.path, 'channel')) errors.channel = true;
		if (hasIssueForPath(issue.path, 'max_clients')) errors.max_clients = true;
		if (hasIssueForPath(issue.path, 'local_ip')) errors.local_ip = true;
		if (hasIssueForPath(issue.path, 'gateway_ip')) errors.gateway_ip = true;
		if (hasIssueForPath(issue.path, 'subnet_mask')) errors.subnet_mask = true;
	}

	return { valid: false, errors };
}

export function validateWifiStaSettings(settings: WifiSettings): {
	valid: boolean;
	errors: WifiStaSettingsErrors;
} {
	const errors = createWifiStaSettingsErrors();
	const result = WifiStaSettingsSchema.safeParse(settings);

	if (result.success) {
		return { valid: true, errors };
	}

	for (const issue of result.error.issues) {
		if (hasIssueForPath(issue.path, 'hostname')) {
			errors.hostname = true;
			continue;
		}

		if (hasIssueForPath(issue.path, 'wifi_networks')) {
			errors.wifi_networks = true;
		}
	}

	return { valid: false, errors };
}

export function validateWifiNetwork(network: KnownNetworkItem) {
	return WifiNetworkSchema.safeParse({
		ssid: network.ssid ?? '',
		password: network.password ?? '',
		static_ip_config: Boolean(network.static_ip_config),
		local_ip: network.local_ip ?? '',
		subnet_mask: network.subnet_mask ?? '',
		gateway_ip: network.gateway_ip ?? '',
		dns_ip_1: network.dns_ip_1 ?? '',
		dns_ip_2: network.dns_ip_2 ?? ''
	});
}
