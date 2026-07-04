/**
 * Common validation utilities
 * Extracted from multiple components to follow DRY principle
 */

export const MAX_WIFI_SSID_BYTES = 32;

/**
 * IPv4 address regex pattern
 * Matches valid IPv4 addresses (0.0.0.0 to 255.255.255.255)
 */
const IPV4_REGEX =
	/^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/;

const UTF8_ENCODER = new TextEncoder();

/**
 * Validate IPv4 address format
 * @param ip - IP address string to validate
 */
export function isValidIPv4(ip: string | undefined | null): boolean {
	if (!ip || typeof ip !== 'string') return false;
	return IPV4_REGEX.test(ip.trim());
}

function getUtf8ByteLength(value: string | undefined | null): number {
	if (typeof value !== 'string') return 0;
	return UTF8_ENCODER.encode(value).length;
}

/**
 * Validate SSID (WiFi network name)
 * Must be 1-32 UTF-8 bytes to match firmware-side validation
 * @param ssid - SSID string to validate
 * @param minBytes - Minimum byte length (default: 1)
 */
export function isValidSSID(ssid: string | undefined | null, minBytes: number = 1): boolean {
	const byteLength = getUtf8ByteLength(ssid);
	return byteLength >= minBytes && byteLength <= MAX_WIFI_SSID_BYTES;
}

/**
 * Validate WiFi password
 * Must be empty (open network) or 8-63 characters for WPA/WPA2
 * @param password - Password string to validate
 */
export function isValidWifiPassword(password: string | undefined | null): boolean {
	if (password === undefined || password === null) return true;
	if (typeof password !== 'string') return false;
	// Empty password is valid (open network)
	if (password.length === 0) return true;
	// WPA/WPA2 requires 8-63 characters
	return password.length >= 8 && password.length <= 63;
}

/**
 * Validate Telegram bot token format
 * Format: 123456789:ABCdef...
 * @param token - Bot token string to validate
 */
export function isValidTelegramBotToken(token: string | undefined | null): boolean {
	if (!token || typeof token !== 'string') return false;
	const trimmed = token.trim();
	// Basic format check: number:alphanumeric
	return /^\d+:[A-Za-z0-9_-]+$/.test(trimmed) && trimmed.length >= 10;
}

/**
 * Validate HTTP/HTTPS URL format
 * @param url - URL string to validate
 */
export function isValidHttpUrl(url: string | undefined | null): boolean {
	if (!url || typeof url !== 'string') return false;
	try {
		const parsed = new URL(url.trim());
		return parsed.protocol === 'http:' || parsed.protocol === 'https:';
	} catch {
		return false;
	}
}
