<script lang="ts">
	import SavedDeviceList from './SavedDeviceList.svelte';
	import ScanResultsModal from './ScanResultsModal.svelte';
	import { useBleScanner } from './useBleScanner.svelte';
	import { bluetoothStore } from '$lib/stores/bluetooth.svelte';

	import type { BleApiService } from '$lib/services/api/connectivity/BleApiService';

	let { api, canManage = true } = $props<{
		api: BleApiService;
		canManage?: boolean;
	}>();

	const scanner = useBleScanner(() => api);

	let savedSettings = $derived(bluetoothStore.settings);
	let scannerEnabled = $derived(savedSettings?.enabled ?? false);
</script>

<!-- Main List -->
<SavedDeviceList
	{savedSettings}
	{scannerEnabled}
	{canManage}
	myDevices={scanner.myDevices}
	now={scanner.now}
	onStartScan={scanner.startScan}
	onRemoveDevice={scanner.removeDevice}
	onEditDevice={scanner.editDevice}
/>

<!-- Scanning Modal -->
{#if canManage}
	<ScanResultsModal
		isOpen={scanner.showScanModal}
		isScanning={scanner.isScanning}
		scanTimeLeft={scanner.scanTimeLeft}
		scanResults={scanner.scanResults}
		{savedSettings}
		onStartScan={scanner.startScan}
		onCloseScan={scanner.closeScan}
		onAddDevice={scanner.addDevice}
	/>
{/if}
