import { TIME_ZONES } from '$lib/data/timezones';

const regexExpIPv4 =
	/\b(?:(?:2(?:[0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9])\.){3}(?:(?:2([0-4][0-9]|5[0-5])|[0-1]?[0-9]?[0-9]))\b/;
const regexExpURL = /[-a-zA-Z0-9@:%_.~#?&//=]{2,256}\.[a-z]{2,4}\b(\/[-a-zA-Z0-9@:%_.~#?&//=]*)?/i;

export interface NtpValidationResult {
	valid: boolean;
	errors: {
		server: boolean;
	};
}

export function validateNtpSettings(enabled: boolean, server: string): NtpValidationResult {
	const errors = { server: false };
	let valid = true;

	if (enabled) {
		if (!regexExpURL.test(server) && !regexExpIPv4.test(server)) {
			valid = false;
			errors.server = true;
		}
	}

	return { valid, errors };
}

export function formatTimezoneFromLabel(tz_label: string): string {
	return TIME_ZONES[tz_label];
}

export function getBrowserTime(): string {
	const now = new Date();
	const year = now.getFullYear();
	const month = String(now.getMonth() + 1).padStart(2, '0');
	const day = String(now.getDate()).padStart(2, '0');
	const hours = String(now.getHours()).padStart(2, '0');
	const minutes = String(now.getMinutes()).padStart(2, '0');
	const seconds = String(now.getSeconds()).padStart(2, '0');
	return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}`;
}
