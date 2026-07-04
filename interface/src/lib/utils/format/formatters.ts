/**
 * Common formatting utilities
 * Extracted from multiple components to follow DRY principle
 */

/**
 * Format bytes to human readable string (KB, MB, GB)
 * @param bytes - Number of bytes to format
 * @param decimals - Number of decimal places (default: 1)
 */
export function formatBytes(bytes: number, decimals: number = 1): string {
	if (bytes === 0) return '0 B';
	if (bytes < 0) bytes = Math.abs(bytes);

	if (bytes >= 1024 * 1024 * 1024) {
		return `${(bytes / (1024 * 1024 * 1024)).toFixed(decimals)} GB`;
	}
	if (bytes >= 1024 * 1024) {
		return `${(bytes / (1024 * 1024)).toFixed(decimals)} MB`;
	}
	if (bytes >= 1024) {
		return `${(bytes / 1024).toFixed(decimals)} KB`;
	}
	return `${bytes} B`;
}

/**
 * Format seconds to human readable duration string (Xd Xh Xm Xs)
 * @param seconds - Total seconds to format
 * @param compact - If true, returns shorter format without spaces
 */
export function formatDuration(seconds: number, compact: boolean = false): string {
	if (seconds < 0) seconds = Math.abs(seconds);

	let minutes = Math.floor(seconds / 60);
	let hours = Math.floor(minutes / 60);
	const days = Math.floor(hours / 24);

	hours = hours % 24;
	minutes = minutes % 60;
	seconds = Math.floor(seconds % 60);

	const parts: string[] = [];
	const sep = compact ? '' : ' ';

	if (days > 0) parts.push(`${days}d`);
	if (hours > 0) parts.push(`${hours}h`);
	if (minutes > 0) parts.push(`${minutes}m`);
	if (seconds > 0 || parts.length === 0) parts.push(`${seconds}s`);

	return parts.join(sep);
}

/**
 * Format temperature with unit
 * @param celsius - Temperature in Celsius
 * @param invalidValue - Value that indicates invalid reading (default: 53.33 for ESP32)
 */
export function formatTemperature(celsius: number, invalidValue: number = 53.33): string {
	if (celsius === invalidValue) return 'N/A';
	return `${celsius.toFixed(2)} °C`;
}

/**
 * Escape HTML special characters to prevent XSS
 * @param value - String to escape
 */
export function escapeHtml(value: string): string {
	return value
		.replaceAll('&', '&amp;')
		.replaceAll('<', '&lt;')
		.replaceAll('>', '&gt;')
		.replaceAll('"', '&quot;')
		.replaceAll("'", '&#039;');
}
