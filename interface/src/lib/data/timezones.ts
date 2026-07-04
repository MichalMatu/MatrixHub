/**
 * Reduced timezone list containing ~80 most commonly used zones.
 * Original had 400+ entries. If you need more, add them here.
 *
 * Format: 'Region/City': 'POSIX TZ string'
 */

export const TIME_ZONES: Record<string, string> = {
	// === EUROPE (most common) ===
	'Europe/Amsterdam': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Athens': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
	'Europe/Belgrade': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Berlin': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Brussels': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Bucharest': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
	'Europe/Budapest': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Copenhagen': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Dublin': 'IST-1GMT0,M10.5.0,M3.5.0/1',
	'Europe/Helsinki': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
	'Europe/Istanbul': 'UNK-3',
	'Europe/Kyiv': 'EET-2EEST,M3.5.0/3,M10.5.0/4',
	'Europe/Lisbon': 'WET0WEST,M3.5.0/1,M10.5.0',
	'Europe/London': 'GMT0BST,M3.5.0/1,M10.5.0',
	'Europe/Madrid': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Moscow': 'MSK-3',
	'Europe/Oslo': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Paris': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Prague': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Rome': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Stockholm': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Vienna': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Warsaw': 'CET-1CEST,M3.5.0,M10.5.0/3',
	'Europe/Zurich': 'CET-1CEST,M3.5.0,M10.5.0/3',

	// === NORTH AMERICA ===
	'America/Anchorage': 'AKST9AKDT,M3.2.0,M11.1.0',
	'America/Chicago': 'CST6CDT,M3.2.0,M11.1.0',
	'America/Denver': 'MST7MDT,M3.2.0,M11.1.0',
	'America/Los_Angeles': 'PST8PDT,M3.2.0,M11.1.0',
	'America/New_York': 'EST5EDT,M3.2.0,M11.1.0',
	'America/Phoenix': 'MST7',
	'America/Toronto': 'EST5EDT,M3.2.0,M11.1.0',
	'America/Vancouver': 'PST8PDT,M3.2.0,M11.1.0',
	'America/Mexico_City': 'CST6CDT,M4.1.0,M10.5.0',
	'America/Tijuana': 'PST8PDT,M3.2.0,M11.1.0',

	// === SOUTH AMERICA ===
	'America/Argentina/Buenos_Aires': 'UNK3',
	'America/Bogota': 'UNK5',
	'America/Caracas': 'UNK4',
	'America/Lima': 'UNK5',
	'America/Santiago': 'UNK4UNK,M9.1.6/24,M4.1.6/24',
	'America/Sao_Paulo': 'UNK3',

	// === AFRICA ===
	'Africa/Cairo': 'EET-2',
	'Africa/Johannesburg': 'SAST-2',
	'Africa/Lagos': 'WAT-1',
	'Africa/Nairobi': 'EAT-3',

	// === ASIA ===
	'Asia/Bangkok': 'UNK-7',
	'Asia/Beijing': 'CST-8',
	'Asia/Calcutta': 'IST-5:30',
	'Asia/Dubai': 'UNK-4',
	'Asia/Hong_Kong': 'HKT-8',
	'Asia/Jakarta': 'WIB-7',
	'Asia/Jerusalem': 'IST-2IDT,M3.4.4/26,M10.5.0',
	'Asia/Kathmandu': 'UNK-5:45',
	'Asia/Kolkata': 'IST-5:30',
	'Asia/Kuala_Lumpur': 'UNK-8',
	'Asia/Manila': 'PST-8',
	'Asia/Riyadh': 'UNK-3',
	'Asia/Seoul': 'KST-9',
	'Asia/Shanghai': 'CST-8',
	'Asia/Singapore': 'UNK-8',
	'Asia/Taipei': 'CST-8',
	'Asia/Tehran': 'UNK-3:30UNK,J79/24,J263/24',
	'Asia/Tokyo': 'JST-9',

	// === OCEANIA ===
	'Australia/Adelaide': 'ACST-9:30ACDT,M10.1.0,M4.1.0/3',
	'Australia/Brisbane': 'AEST-10',
	'Australia/Darwin': 'ACST-9:30',
	'Australia/Hobart': 'AEST-10AEDT,M10.1.0,M4.1.0/3',
	'Australia/Melbourne': 'AEST-10AEDT,M10.1.0,M4.1.0/3',
	'Australia/Perth': 'AWST-8',
	'Australia/Sydney': 'AEST-10AEDT,M10.1.0,M4.1.0/3',
	'Pacific/Auckland': 'NZST-12NZDT,M9.5.0,M4.1.0/3',
	'Pacific/Fiji': 'UNK-12UNK,M11.2.0,M1.2.3/99',
	'Pacific/Honolulu': 'HST10',

	// === MISC ===
	UTC: 'UTC0',
	GMT: 'GMT0'
};
