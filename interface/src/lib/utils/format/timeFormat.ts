/**
 * Utility functions for consistent time formatting across the application
 */
import { Logger } from '$lib/services/core/Logger';

const MONTH_NAMES = [
	'January',
	'February',
	'March',
	'April',
	'May',
	'June',
	'July',
	'August',
	'September',
	'October',
	'November',
	'December'
];

type ParsedIso = {
	year: number;
	month: number;
	day: number;
	hour: number;
	minute: number;
	second: number;
	offset: string | null; // "+01:00" or "Z" or null when missing
};

function parseIsoWithOffset(value: string): ParsedIso | null {
	const m = value.match(
		/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:(Z)|([+-])(\d{2}):?(\d{2}))?$/
	);
	if (!m) return null;

	const [, y, mo, d, h, mi, s, zFlag, sign, oh, om] = m;
	let offset: string | null = null;
	if (zFlag === 'Z') {
		offset = 'Z';
	} else if (sign && oh && om) {
		offset = `${sign}${oh}:${om}`;
	}

	return {
		year: Number(y),
		month: Number(mo),
		day: Number(d),
		hour: Number(h),
		minute: Number(mi),
		second: Number(s),
		offset
	};
}

function monthName(idx: number): string {
	return MONTH_NAMES[idx - 1] ?? '';
}

function pad2(n: number): string {
	return String(n).padStart(2, '0');
}

function formatOffset(offset: string | null, tzLabel?: string): string {
	if (tzLabel) return tzLabel;
	if (!offset) return '';
	return offset === 'Z' ? 'UTC' : `UTC${offset}`;
}

/**
 * Format a date/time string for long display format without relying on browser timezone.
 * Preserves the wall time encoded in the string; uses tzLabel if provided, otherwise offset.
 */
export function formatLongDateTime(dateString: string, tzLabel?: string): string {
	try {
		const parsed = parseIsoWithOffset(dateString);
		if (!parsed) throw new Error('invalid ISO');
		const label = formatOffset(parsed.offset, tzLabel);
		const base = `${parsed.day} ${monthName(parsed.month)} ${parsed.year} at ${pad2(parsed.hour)}:${pad2(parsed.minute)}:${pad2(parsed.second)}`;
		return label ? `${base} ${label}` : base;
	} catch (error) {
		Logger.error('Error formatting date:', dateString, error);
		return 'Invalid date';
	}
}

/**
 * Format a date/time string for UTC display (kept as real UTC conversion).
 */
export function formatUTCDateTime(dateString: string): string {
	try {
		const date = new Date(dateString);
		return new Intl.DateTimeFormat('en-GB', {
			dateStyle: 'long',
			timeStyle: 'long',
			timeZone: 'UTC'
		}).format(date);
	} catch (error) {
		Logger.error('Error formatting UTC date:', dateString, error);
		return 'Invalid date';
	}
}
