import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen } from '@testing-library/svelte';
import BleWidget from './BleWidget.svelte';

const { mockController, mockSession } = vi.hoisted(() => ({
	mockController: {
		widgetError: null as string | null,
		hasSensors: false,
		sensors: [],
		scannerEnabled: true,
		settingsLoading: false,
		getTimeSince: vi.fn(() => '5s ago'),
		getBatteryClass: vi.fn(() => 'text-success'),
		getRssiClass: vi.fn(() => 'text-success')
	},
	mockSession: {
		canManage: false
	}
}));

vi.mock('./useBleWidgetManagement.svelte', () => ({
	useBleWidgetManagement: () => mockController
}));

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => mockSession
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	widget_ble_title: () => 'Bluetooth Gateway',
	widget_ble_disabled: () => 'Scanner disabled',
	widget_ble_no_sensors: () => 'No sensors configured',
	widget_ble_tap_add: () => 'Tap to add devices',
	widget_ble_tap_open: () => 'Open Bluetooth to review devices',
	status_waiting: () => 'Waiting for data...'
}));

describe('BleWidget', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		mockController.widgetError = null;
		mockController.hasSensors = false;
		mockController.sensors = [];
		mockController.scannerEnabled = true;
		mockController.settingsLoading = false;
		mockSession.canManage = false;
	});

	it('links read-only users to the Bluetooth page', () => {
		render(BleWidget);

		expect(screen.getByRole('link').getAttribute('href')).toBe('/bluetooth');
		expect(screen.getByText('Open Bluetooth to review devices')).toBeTruthy();
	});

	it('keeps add-oriented helper copy for administrators', () => {
		mockSession.canManage = true;

		render(BleWidget);

		expect(screen.getByText('Tap to add devices')).toBeTruthy();
	});
});
