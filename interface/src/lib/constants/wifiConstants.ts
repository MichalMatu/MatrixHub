import * as m from '$lib/paraglide/messages.js';

const WIFI_AUTH_OPEN = 0;
const WIFI_AUTH_WEP = 1;
const WIFI_AUTH_WPA_PSK = 2;
const WIFI_AUTH_WPA2_PSK = 3;
const WIFI_AUTH_WPA_WPA2_PSK = 4;
const WIFI_AUTH_WPA2_ENTERPRISE = 5;
const WIFI_AUTH_WPA3_PSK = 6;
const WIFI_AUTH_WPA2_WPA3_PSK = 7;
const WIFI_AUTH_WAPI_PSK = 8;

/**
 * Get the display name for a given WiFi encryption type index.
 * @param typeIndex - The encryption type index from the scan result.
 * @param languageTag - The current language tag for localization.
 * @returns Localized string decription of the security mode.
 */
export function getEncryptionTypeLabel(typeIndex: number, languageTag: string): string {
	switch (typeIndex) {
		case WIFI_AUTH_OPEN:
			return m.wifi_encryption_open({ locale: languageTag });
		case WIFI_AUTH_WEP:
			return 'WEP';
		case WIFI_AUTH_WPA_PSK:
			return 'WPA PSK';
		case WIFI_AUTH_WPA2_PSK:
			return 'WPA2 PSK';
		case WIFI_AUTH_WPA_WPA2_PSK:
			return 'WPA WPA2 PSK';
		case WIFI_AUTH_WPA2_ENTERPRISE:
			return 'WPA2 Enterprise';
		case WIFI_AUTH_WPA3_PSK:
			return 'WPA3 PSK';
		case WIFI_AUTH_WPA2_WPA3_PSK:
			return 'WPA2 WPA3 PSK';
		case WIFI_AUTH_WAPI_PSK:
			return 'WAPI PSK';
		default:
			return 'Unknown';
	}
}

export const WIFI_SCAN_POLL_INTERVAL_MS = 1000;
