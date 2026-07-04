import { onMount, onDestroy, untrack } from 'svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import type { BleSettings, BleSensorConfig } from '$lib/types/connectivity/ble';
import { BleApiService } from '$lib/services/api/connectivity/BleApiService';
import { Logger } from '$lib/services/core/Logger';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { confirm, prompt } from '$lib/utils/ui/dialogs';
import { bluetoothStore } from '$lib/stores/bluetooth.svelte';
import { normalizeMac } from '$lib/utils/ble';

// Icons
import Trash from '~icons/tabler/trash';
import X from '~icons/tabler/x';

export interface ScannedDevice {
	mac: string;
	temp: number;
	humid: number;
	batt: number;
	rssi: number;
	lastSeen: number;
	alias?: string;
	saved?: boolean; // Added optional property
}

interface ScannerCollections {
	liveData: Record<string, ScannedDevice>;
	scanResults: Record<string, ScannedDevice>;
}

const SCAN_TIMEOUT_MS = 30000;
const SCAN_COUNTDOWN_SEC = 30;
const TICK_INTERVAL_MS = 1000;
const ALIAS_SUFFIX_LENGTH = 5;
const MAX_ALIAS_LEN = 23; // BLE alias max: kMaxDeviceNameLen (24) - 1
const NOTIFICATION_DURATION_MS = 3000;

export function reconcileScannerData(
	devices: Record<string, ScannedDevice>,
	savedSettings: BleSettings | null,
	modalOpen: boolean,
	previousScanResults: Record<string, ScannedDevice>,
	nowTimestamp = Date.now()
): ScannerCollections {
	const nextLiveData: Record<string, ScannedDevice> = {};
	const nextScanResults: Record<string, ScannedDevice> = modalOpen ? {} : previousScanResults;

	for (const data of Object.values(devices)) {
		const mac = normalizeMac(data.mac);
		if (!mac) continue;

		const known = savedSettings?.sensors?.find(
			(sensor: BleSensorConfig) => normalizeMac(sensor.mac) === mac
		);
		const isSaved = Boolean(known);
		const device: ScannedDevice = {
			mac,
			temp: data.temp ?? 0,
			humid: data.humid ?? 0,
			batt: data.batt ?? 0,
			rssi: data.rssi ?? 0,
			lastSeen: data.lastSeen ?? nowTimestamp,
			alias: known?.alias ?? data.alias
		};

		if (isSaved) {
			nextLiveData[mac] = device;
		}

		if (modalOpen) {
			nextScanResults[mac] = {
				...device,
				saved: Boolean(isSaved || previousScanResults[mac]?.saved)
			};
		}
	}

	return {
		liveData: nextLiveData,
		scanResults: nextScanResults
	};
}

export function useBleScanner(getApi: () => BleApiService) {
	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}

	function normalizeAlias(value: string): string {
		return filterAsciiPrintable(value).slice(0, MAX_ALIAS_LEN);
	}

	// Main list (Whitelisted devices data)
	let liveData = $state<Record<string, ScannedDevice>>({});

	// Modal scan results
	let showScanModal = $state(false);
	let scanResults = $state<Record<string, ScannedDevice>>({});
	let isScanning = $state(false);
	let scanTimeLeft = $state(0);
	let stopScanPromise: Promise<void> | null = null;

	// Reactive timestamp for relative time updates
	let now = $state(Date.now());
	let tickInterval: ReturnType<typeof setInterval>;

	function updateTick() {
		if (!tickInterval) return; // Safeguard against ghost executions
		untrack(() => {
			now = Date.now();
			if (isScanning && scanTimeLeft > 0) {
				scanTimeLeft--;
				if (scanTimeLeft <= 0) {
					void stopScan();
				}
			}
		});
	}

	function updateDataFromStore(
		devices: Record<string, ScannedDevice>,
		savedSettings: BleSettings | null,
		modalOpen: boolean
	) {
		const nextState = reconcileScannerData(
			devices,
			savedSettings,
			modalOpen,
			scanResults,
			Date.now()
		);

		liveData = nextState.liveData;
		if (modalOpen) {
			scanResults = nextState.scanResults;
		}
	}

	$effect(() => {
		const devices = bluetoothStore.devices as unknown as Record<string, ScannedDevice>;
		const savedSettings = bluetoothStore.settings;
		const modalOpen = showScanModal;

		untrack(() => {
			updateDataFromStore(devices, savedSettings, modalOpen);
		});
	});

	onDestroy(() => {
		if (tickInterval) clearInterval(tickInterval);
	});

	async function startScan() {
		showScanModal = true;
		scanResults = {};
		isScanning = true;
		scanTimeLeft = SCAN_COUNTDOWN_SEC;
		try {
			await getApi().startScan(SCAN_TIMEOUT_MS);
			bluetoothStore.refresh();
			notifications.success(
				m.ble_msg_scanner_enabled({ locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		} catch (e) {
			const kind = getRequestAbortKind(e);
			isScanning = false;
			if (kind === 'abort') return;
			Logger.error('Failed to start BLE scan', e);
			const msg = toUserRequestErrorMessage(e, {
				timeoutMessage: m.toast_ble_scan_start_timeout({ locale: i18n.languageTag }),
				fallbackMessage: m.toast_ble_scan_start_failed({ locale: i18n.languageTag })
			});
			notifications.error(m.toast_message({ message: msg }, { locale: i18n.languageTag }), 5000);
		}
	}

	async function stopScan() {
		isScanning = false;
		if (stopScanPromise) {
			await stopScanPromise;
			return;
		}

		stopScanPromise = (async () => {
			try {
				await getApi().stopScan();
				bluetoothStore.refresh();
			} catch (e) {
				Logger.warn('Failed to stop BLE scan', e);
			}
		})();

		try {
			await stopScanPromise;
		} finally {
			stopScanPromise = null;
		}
	}

	async function closeScan() {
		await stopScan();
		showScanModal = false;
	}

	async function saveSensors(sensors: BleSensorConfig[]) {
		const settings = await getApi().saveSettings({ sensors });
		bluetoothStore.setSettings(settings);
	}

	async function addDevice(device: ScannedDevice) {
		const savedSettings = bluetoothStore.settings;
		const currentSensors = savedSettings?.sensors ? [...savedSettings.sensors] : [];
		const mac = normalizeMac(device.mac);
		if (!mac) return;
		if (currentSensors.find((s: BleSensorConfig) => normalizeMac(s.mac) === mac)) return;

		currentSensors.push({
			mac,
			alias: normalizeAlias(
				m.ble_default_alias({ locale: i18n.languageTag }) + ' ' + mac.slice(-ALIAS_SUFFIX_LENGTH)
			)
		});

		try {
			await saveSensors(currentSensors);
			if (scanResults[mac]) {
				scanResults[mac] = { ...scanResults[mac], saved: true };
			}
			const existing = bluetoothStore.devices[mac];
			if (existing) {
				bluetoothStore.updateDevice({ ...existing, saved: true });
			}
			notifications.success(
				m.toast_ble_device_added({ locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		} catch (e) {
			Logger.error('Failed to save BLE device list', e);
			notifications.error(
				m.ble_error_settings_save_fallback({ locale: i18n.languageTag }),
				NOTIFICATION_DURATION_MS
			);
		}
	}

	function removeDevice(mac: string) {
		const targetMac = normalizeMac(mac);
		if (!targetMac) return;
		confirm({
			title: m.ble_dialog_forget_title({ locale: i18n.languageTag }),
			message: m.ble_dialog_forget_msg({ mac }, { locale: i18n.languageTag }),
			labels: {
				cancel: { label: m.action_cancel({ locale: i18n.languageTag }), icon: X },
				confirm: { label: m.action_delete({ locale: i18n.languageTag }), icon: Trash }
			},
			onConfirm: async () => {
				// Small delay for modal transition after ConfirmDialog closes itself.
				await new Promise((resolve) => setTimeout(resolve, 50));

				const savedSettings = bluetoothStore.settings;
				const currentSensors = savedSettings?.sensors ? [...savedSettings.sensors] : [];
				const filtered = currentSensors.filter(
					(s: BleSensorConfig) => normalizeMac(s.mac) !== targetMac
				);

				try {
					await saveSensors(filtered);
					delete liveData[targetMac];
					if (scanResults[targetMac]) {
						scanResults[targetMac] = { ...scanResults[targetMac], saved: false };
					}
					const existing = bluetoothStore.devices[targetMac];
					if (existing) {
						bluetoothStore.updateDevice({ ...existing, saved: false });
					}
					notifications.success(
						m.toast_ble_device_removed({ locale: i18n.languageTag }),
						NOTIFICATION_DURATION_MS
					);
				} catch (e) {
					Logger.error('Failed to remove BLE device', e);
					notifications.error(
						m.ble_error_settings_save_fallback({ locale: i18n.languageTag }),
						NOTIFICATION_DURATION_MS
					);
				}
			}
		});
	}

	onMount(() => {
		tickInterval = setInterval(updateTick, TICK_INTERVAL_MS);
	});

	// Reactively update data when status changes (passed from parent)
	// OLD Status based update removed in favor of Store
	// $effect(() => {
	// 	const status = getBleStatus();
	// 	if (status) {
	// 		untrack(() => updateDataFromStatus(status));
	// 	}
	// });

	let myDevices = $derived.by(() => {
		const savedSettings = bluetoothStore.settings;
		if (!savedSettings?.sensors) return [];
		return savedSettings.sensors.map((cfg: BleSensorConfig) => {
			const data = liveData[normalizeMac(cfg.mac)];
			return {
				config: cfg,
				data: data
			};
		});
	});

	return {
		get liveData() {
			return liveData;
		},
		get showScanModal() {
			return showScanModal;
		},
		get scanResults() {
			return scanResults;
		},
		get isScanning() {
			return isScanning;
		},
		get scanTimeLeft() {
			return scanTimeLeft;
		},
		get now() {
			return now;
		},
		get myDevices() {
			return myDevices;
		},

		startScan,
		stopScan,
		closeScan,
		addDevice,
		removeDevice,
		editDevice
	};

	function editDevice(mac: string, currentAlias: string) {
		const targetMac = normalizeMac(mac);
		if (!targetMac) return;
		prompt({
			title: m.action_edit({ locale: i18n.languageTag }),
			message: m.ble_dialog_edit_alias_msg({ mac }, { locale: i18n.languageTag }),
			defaultValue: currentAlias,
			maxlength: MAX_ALIAS_LEN,
			asciiOnly: true,
			onConfirm: async (newAlias: string) => {
				const alias = normalizeAlias(newAlias.trim());
				if (!alias || alias === currentAlias) return;

				const savedSettings = bluetoothStore.settings;
				const currentSensors = savedSettings?.sensors ? [...savedSettings.sensors] : [];
				const sensor = currentSensors.find(
					(s: BleSensorConfig) => normalizeMac(s.mac) === targetMac
				);

				if (sensor) {
					sensor.alias = alias;

					try {
						await saveSensors(currentSensors);
						notifications.success(
							m.toast_ble_device_updated({ locale: i18n.languageTag }),
							NOTIFICATION_DURATION_MS
						);
					} catch (e) {
						Logger.error('Failed to update BLE device alias', e);
						notifications.error(
							m.ble_error_settings_save_fallback({ locale: i18n.languageTag }),
							NOTIFICATION_DURATION_MS
						);
					}
				}
			}
		});
	}
}
