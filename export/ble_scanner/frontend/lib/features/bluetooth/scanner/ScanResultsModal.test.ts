import { fireEvent, render, screen } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import ScanResultsModal from './ScanResultsModal.svelte';
import type { BleSettings } from '$lib/types/connectivity/ble';

interface TestScannedDevice {
	mac: string;
	temp: number;
	humid: number;
	batt: number;
	rssi: number;
	lastSeen: number;
	saved?: boolean;
}

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	ble_scan_title: () => 'Scan devices',
	ble_scanning_msg: () => 'Scanning...',
	ble_scan_no_devices: () => 'No devices',
	dashboard_humid: () => 'Humidity',
	tooltip_battery: () => 'Battery',
	tooltip_signal: () => 'Signal',
	ble_scan_btn_add: () => 'Add',
	ble_scan_added: () => 'Added',
	action_close: () => 'Close',
	ble_scan_again: () => 'Scan again'
}));

function renderModal(options?: {
	scanResults?: Record<string, TestScannedDevice>;
	savedSettings?: BleSettings | null;
	onAddDevice?: (device: TestScannedDevice) => void;
	onCloseScan?: () => void;
}) {
	return render(ScanResultsModal, {
		props: {
			isOpen: true,
			isScanning: false,
			scanTimeLeft: 0,
			scanResults: options?.scanResults ?? {},
			savedSettings: options?.savedSettings ?? null,
			onStartScan: vi.fn(),
			onCloseScan: options?.onCloseScan ?? vi.fn(),
			onAddDevice: options?.onAddDevice ?? vi.fn()
		}
	});
}

describe('ScanResultsModal', () => {
	it('does not close modal when adding a device', async () => {
		const onAddDevice = vi.fn();
		const onCloseScan = vi.fn();
		renderModal({
			scanResults: {
				'11:22:33:44:55:66': {
					mac: '11:22:33:44:55:66',
					temp: 21.5,
					humid: 50,
					batt: 80,
					rssi: -45,
					lastSeen: Date.now()
				}
			},
			onAddDevice,
			onCloseScan
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Add' }));

		expect(onAddDevice).toHaveBeenCalledWith(expect.objectContaining({ mac: '11:22:33:44:55:66' }));
		expect(onCloseScan).not.toHaveBeenCalled();
		expect(screen.getByText('11:22:33:44:55:66')).toBeTruthy();
	});

	it('shows Added state for already saved devices', () => {
		renderModal({
			scanResults: {
				'11:22:33:44:55:66': {
					mac: '11:22:33:44:55:66',
					temp: 21.5,
					humid: 50,
					batt: 80,
					rssi: -45,
					lastSeen: Date.now(),
					saved: true
				}
			}
		});

		expect(screen.getByText('Added')).toBeTruthy();
		expect(screen.queryByRole('button', { name: 'Add' })).toBeNull();
	});

	it('matches saved settings case-insensitively', () => {
		renderModal({
			scanResults: {
				'aa:bb:cc:dd:ee:ff': {
					mac: 'aa:bb:cc:dd:ee:ff',
					temp: 21.5,
					humid: 50,
					batt: 80,
					rssi: -45,
					lastSeen: Date.now()
				}
			},
			savedSettings: {
				sensors: [{ mac: 'AA:BB:CC:DD:EE:FF', alias: 'Saved sensor' }]
			} as BleSettings
		});

		expect(screen.getByText('Added')).toBeTruthy();
		expect(screen.queryByRole('button', { name: 'Add' })).toBeNull();
	});

	it('keeps horizontal overflow disabled for scan results list', () => {
		renderModal({
			scanResults: {
				'11:22:33:44:55:66': {
					mac: '11:22:33:44:55:66',
					temp: 21.5,
					humid: 50,
					batt: 80,
					rssi: -45,
					lastSeen: Date.now()
				}
			}
		});

		const scrollContainer = document.querySelector('.overflow-x-hidden');
		const resultRow = screen.getByText('11:22:33:44:55:66').closest('div[class*="rounded-btn"]');

		expect(scrollContainer).toBeTruthy();
		expect(resultRow?.className).not.toContain('hover:scale');
	});
});
