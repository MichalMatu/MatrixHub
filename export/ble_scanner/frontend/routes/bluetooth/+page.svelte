<script lang="ts">
	import { useBluetoothManagement } from '$lib/features/bluetooth/useBluetoothManagement.svelte';
	import { PageWrapper } from '$lib/components/layout';

	import BleStatusCard from '$lib/features/bluetooth/status/BleStatus.svelte';
	import BleSettingsCard from '$lib/features/bluetooth/settings/BleSettings.svelte';
	import BleScanner from '$lib/features/bluetooth/scanner/BleScanner.svelte';

	const bluetooth = useBluetoothManagement();
	const canManage = $derived(bluetooth.canManage);
</script>

<PageWrapper>
	<!-- Error -->
	{#if bluetooth.statusError}
		<div class="alert alert-error mb-4">
			<span>{bluetooth.statusError}</span>
		</div>
	{/if}
	{#if canManage && bluetooth.settingsError}
		<div class="alert alert-warning mb-4">
			<span>{bluetooth.settingsError}</span>
		</div>
	{/if}

	<div class="grid grid-cols-1 md:grid-cols-2 gap-4">
		<div class={canManage ? 'order-2 md:order-1 md:col-span-2' : 'order-1 md:col-span-2'}>
			<BleScanner api={bluetooth.api} {canManage} />
		</div>

		<div class={canManage ? 'order-3 md:order-2' : 'md:col-span-2'}>
			<BleStatusCard isRunning={bluetooth.isRunning} isScannerActive={bluetooth.isScannerActive} />
		</div>

		{#if canManage}
			<div class="order-1 md:order-3">
				<BleSettingsCard
					savedSettings={bluetooth.savedSettings}
					bind:localEnabled={bluetooth.localEnabled}
					hasChanges={bluetooth.hasChanges}
					saving={bluetooth.saving}
					onSave={() => bluetooth.confirmSave()}
				/>
			</div>
		{/if}
	</div>
</PageWrapper>
