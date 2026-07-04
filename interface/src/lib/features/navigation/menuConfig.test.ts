import { describe, expect, it, vi } from 'vitest';
import { createMenuStructure, type MenuItem } from './menuConfig';

vi.mock('./menuIcons', () => ({
	menuIcons: new Proxy(
		{},
		{
			get: () => ({})
		}
	)
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	menu_charts: () => 'Charts',
	menu_alarms: () => 'Alarms',
	menu_wifi: () => 'WiFi',
	menu_shelly: () => 'Shelly',
	menu_wifisensing: () => 'WiFi Sensing',
	menu_wifi_csi: () => 'WiFi CSI',
	menu_bluetooth: () => 'Bluetooth',
	menu_gpio: () => 'GPIO',
	menu_airmouse: () => 'Airmouse',
	menu_wifi_sta: () => 'WiFi STA',
	menu_wifi_ap: () => 'WiFi AP',
	menu_notifications: () => 'Notifications',
	menu_telegram: () => 'Telegram',
	menu_pushover: () => 'Pushover',
	menu_webhook: () => 'Webhook',
	menu_heartbeat: () => 'Heartbeat',
	menu_udp: () => 'UDP',
	menu_system: () => 'System',
	menu_time: () => 'Time',
	menu_power: () => 'Power',
	menu_users: () => 'Users',
	menu_help: () => 'Help',
	menu_status: () => 'Status',
	menu_logs: () => 'Logs',
	menu_compensation: () => 'Compensation',
	menu_imu: () => 'IMU',
	menu_usb_features: () => 'USB Features',
	menu_mouse_jiggler: () => 'Mouse Jiggler',
	menu_keyboard: () => 'Keyboard',
	menu_macros: () => 'Macros',
	menu_usb_terminal: () => 'USB Terminal',
	menu_matrix_led: () => 'Matrix LED',
	menu_files: () => 'Files',
	menu_styles: () => 'Styles',
	help_alarm_flow_title: () => 'Alarm flow',
	help_troubleshooting_title: () => 'Troubleshooting',
	help_quick_links_setup: () => 'Setup',
	help_quick_links_notify: () => 'Notify',
	help_quick_links_diagnostics: () => 'Diagnostics',
	help_section_integrations_title: () => 'Integrations',
	help_section_system_title: () => 'System',
	menu_locked_admin: () => 'Administrator access required',
	menu_locked_security: () => 'Enable security to unlock this page',
	menu_locked_sta_connection: () => 'Connect Wi-Fi station to unlock',
	menu_locked_feature_unavailable: () => 'Feature unavailable on this device'
}));

function isFeatureEnabled(feature: boolean | (() => boolean)): boolean {
	return typeof feature === 'function' ? feature() : feature;
}

function collectEnabledHrefs(menu: MenuItem[]): string[] {
	const hrefs: string[] = [];

	for (const item of menu) {
		if (!isFeatureEnabled(item.feature)) continue;

		if (item.href && !item.disabledReason) {
			hrefs.push(item.href);
		}

		for (const subItem of item.submenu ?? []) {
			if (isFeatureEnabled(subItem.feature) && !subItem.disabledReason) {
				hrefs.push(subItem.href);
			}
		}
	}

	return hrefs;
}

function collectVisibleTitles(menu: MenuItem[]): string[] {
	const titles: string[] = [];

	for (const item of menu) {
		if (!isFeatureEnabled(item.feature)) continue;
		titles.push(item.title);
	}

	return titles;
}

function findVisibleItem(menu: MenuItem[], title: string): MenuItem | undefined {
	return menu.find((item) => item.title === title && isFeatureEnabled(item.feature));
}

function findSubmenuItem(menu: MenuItem[], parentTitle: string, childTitle: string) {
	const parent = findVisibleItem(menu, parentTitle);
	return parent?.submenu?.find((item) => item.title === childTitle);
}

describe('createMenuStructure', () => {
	it('keeps gated entries visible for non-admin users but disables admin-only actions', () => {
		const menu = createMenuStructure(
			{
				ntpEnabled: true,
				canManage: false,
				isStaConnected: true
			},
			'en',
			'/charts'
		);

		const hrefs = collectEnabledHrefs(menu);
		const titles = collectVisibleTitles(menu);
		const wifiItem = findVisibleItem(menu, 'WiFi');
		const wifiSubmenuTitles = (wifiItem?.submenu ?? [])
			.filter((item) => isFeatureEnabled(item.feature))
			.map((item) => item.title);
		const wifiSta = findSubmenuItem(menu, 'WiFi', 'WiFi STA');
		const wifiAp = findSubmenuItem(menu, 'WiFi', 'WiFi AP');
		const notifications = findVisibleItem(menu, 'Notifications');
		const gpioItem = findVisibleItem(menu, 'GPIO');
		const matrixItem = findVisibleItem(menu, 'Matrix LED');
		const systemItem = findVisibleItem(menu, 'System');
		const systemSubmenuTitles = (systemItem?.submenu ?? [])
			.filter((item) => isFeatureEnabled(item.feature))
			.map((item) => item.title);

		expect(titles).toEqual([
			'Alarms',
			'Notifications',
			'Charts',
			'WiFi',
			'Bluetooth',
			'GPIO',
			'USB Features',
			'Matrix LED',
			'System',
			'Help'
		]);
		expect(wifiSubmenuTitles).toEqual([
			'WiFi STA',
			'WiFi AP',
			'Shelly',
			'WiFi Sensing',
			'WiFi CSI'
		]);
		expect(systemSubmenuTitles).toEqual([
			'Status',
			'Logs',
			'Time',
			'Compensation',
			'IMU',
			'Power',
			'Users',
			'Files',
			'Styles'
		]);
		expect(wifiSta?.disabledReason).toBeUndefined();
		expect(wifiAp?.disabledReason).toBeUndefined();
		expect(matrixItem?.href).toBe('/system/matrix');
		expect(matrixItem?.submenu).toBeUndefined();
		expect(notifications?.disabledReason).toBe('Administrator access required');
		expect(gpioItem?.disabledReason).toBe('Administrator access required');
		expect(hrefs).toContain('/alarms');
		expect(hrefs).toContain('/charts');
		expect(hrefs).toContain('/wifi/sta');
		expect(hrefs).toContain('/wifi/ap');
		expect(hrefs).toContain('/shelly');
		expect(hrefs).toContain('/wifisensing');
		expect(hrefs).toContain('/wifisensing/csi');
		expect(hrefs).toContain('/bluetooth');
		expect(hrefs).not.toContain('/gpio');
		expect(hrefs).toContain('/system/help');
		expect(hrefs).toContain('/system/matrix');
		expect(hrefs).toContain('/logs');
		expect(hrefs).not.toContain('/usb-features/airmouse');
		expect(hrefs).not.toContain('/usb-features/jiggler');
		expect(hrefs).not.toContain('/usb-features/keyboard');
		expect(hrefs).not.toContain('/usb-features/macros');
		expect(hrefs).not.toContain('/settings/notifications/telegram');
	});

	it('keeps WiFi CSI visible but disabled when STA is disconnected', () => {
		const menu = createMenuStructure(
			{
				ntpEnabled: true,
				canManage: false,
				isStaConnected: false
			},
			'en',
			'/wifisensing'
		);

		const hrefs = collectEnabledHrefs(menu);
		const wifiCsi = findSubmenuItem(menu, 'WiFi', 'WiFi CSI');

		expect(wifiCsi?.disabledReason).toBe('Connect Wi-Fi station to unlock');
		expect(hrefs).toContain('/wifisensing');
		expect(hrefs).not.toContain('/wifisensing/csi');
	});

	it('shows full WiFi admin tools and keeps WiFi submenu active on child routes', () => {
		const menu = createMenuStructure(
			{
				ntpEnabled: true,
				canManage: true,
				isStaConnected: true
			},
			'en',
			'/wifisensing/csi'
		);

		const hrefs = collectEnabledHrefs(menu);
		const titles = collectVisibleTitles(menu);
		const wifiItem = findVisibleItem(menu, 'WiFi');
		const wifiSubmenuTitles = (wifiItem?.submenu ?? [])
			.filter((item) => isFeatureEnabled(item.feature))
			.map((item) => item.title);
		const activeWifiChild = wifiItem?.submenu?.find((item) => item.active);

		expect(titles).toEqual([
			'Alarms',
			'Notifications',
			'Charts',
			'WiFi',
			'Bluetooth',
			'GPIO',
			'USB Features',
			'Matrix LED',
			'System',
			'Help'
		]);
		expect(wifiSubmenuTitles).toEqual([
			'WiFi STA',
			'WiFi AP',
			'Shelly',
			'WiFi Sensing',
			'WiFi CSI'
		]);
		expect(wifiItem?.active).toBe(true);
		expect(activeWifiChild?.href).toBe('/wifisensing/csi');
		expect(wifiItem?.submenu?.every((item) => !item.disabledReason)).toBe(true);
		expect(hrefs).toContain('/wifi/sta');
		expect(hrefs).toContain('/wifi/ap');
		expect(hrefs).toContain('/shelly');
		expect(hrefs).toContain('/wifisensing');
		expect(hrefs).toContain('/wifisensing/csi');
		expect(hrefs).toContain('/bluetooth');
		expect(hrefs).toContain('/gpio');
		expect(hrefs).toContain('/usb-features/airmouse');
		expect(hrefs).toContain('/usb-features/jiggler');
		expect(hrefs).toContain('/usb-features/keyboard');
		expect(hrefs).toContain('/usb-features/macros');
		expect(hrefs).toContain('/settings/sensors/imu');
	});

	it('keeps Matrix LED as an active top-level item outside the System submenu', () => {
		const menu = createMenuStructure(
			{
				ntpEnabled: true,
				canManage: true,
				isStaConnected: true
			},
			'en',
			'/system/matrix/effects'
		);

		const matrixItem = findVisibleItem(menu, 'Matrix LED');
		const systemItem = findVisibleItem(menu, 'System');

		expect(matrixItem?.active).toBe(true);
		expect(matrixItem?.href).toBe('/system/matrix');
		expect(matrixItem?.submenu).toBeUndefined();
		expect(systemItem?.active).toBe(false);
		expect(systemItem?.submenu?.map((item) => item.title)).not.toContain('Matrix LED');
	});

	it('marks WiFi as active and highlights Shelly when the Shelly page is open', () => {
		const menu = createMenuStructure(
			{
				ntpEnabled: true,
				canManage: true,
				isStaConnected: true
			},
			'en',
			'/shelly'
		);

		const wifiItem = findVisibleItem(menu, 'WiFi');
		const activeWifiChild = wifiItem?.submenu?.find((item) => item.active);
		const shellyItem = findSubmenuItem(menu, 'WiFi', 'Shelly');

		expect(wifiItem?.active).toBe(true);
		expect(activeWifiChild?.href).toBe('/shelly');
		expect(shellyItem?.active).toBe(true);
		expect(shellyItem?.disabledReason).toBeUndefined();
	});
});
