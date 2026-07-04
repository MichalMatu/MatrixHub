<script lang="ts">
	import DeviceAnalytics from '~icons/tabler/device-analytics';
	import Plus from '~icons/tabler/plus';
	import SavedDeviceItem from './SavedDeviceItem.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface ScannedDevice {
		mac: string;
		temp: number;
		humid: number;
		batt: number;
		rssi: number;
		lastSeen: number;
		alias?: string;
	}

	import type { BleSettings, BleSensorConfig } from '$lib/types/connectivity/ble';

	interface Props {
		savedSettings: BleSettings | null;
		scannerEnabled: boolean;
		canManage?: boolean;
		myDevices: {
			config: BleSensorConfig;
			data: ScannedDevice | undefined;
		}[];
		now: number;
		onStartScan: () => void;
		onRemoveDevice: (_mac: string) => void;
		onEditDevice: (_mac: string, _currentAlias: string) => void;
	}

	let {
		savedSettings,
		scannerEnabled,
		canManage = true,
		myDevices,
		now,
		onStartScan,
		onRemoveDevice,
		onEditDevice
	}: Props = $props();
</script>

<BaseCard
	title={m.ble_my_devices({ locale: i18n.languageTag })}
	icon={DeviceAnalytics}
	class="col-span-1 md:col-span-2 mb-4 transition-opacity duration-300 {scannerEnabled
		? ''
		: 'opacity-60 grayscale'}"
>
	{#snippet actions()}
		{#if canManage}
			<FormButton
				label={m.shelly_btn_add({ locale: i18n.languageTag })}
				icon={Plus}
				disabled={!scannerEnabled}
				onclick={onStartScan}
			/>
		{/if}
	{/snippet}

	<!-- List of Saved Devices -->
	<div class={scannerEnabled ? '' : 'pointer-events-none'}>
		{#if !savedSettings?.sensors || savedSettings.sensors.length === 0}
			<ContentBox
				paddingClass="p-8"
				class="text-center opacity-50 border border-base-300 border-dashed"
			>
				<p>{m.ble_no_paired({ locale: i18n.languageTag })}</p>
				{#if canManage}
					<FormButton
						label={m.ble_scan_to_add({ locale: i18n.languageTag })}
						class="btn-link btn-xs"
						onclick={onStartScan}
						disabled={!scannerEnabled}
					/>
				{/if}
			</ContentBox>
		{:else}
			<div class="grid grid-cols-1 md:grid-cols-2 gap-3">
				{#each myDevices as item (item.config.mac)}
					<SavedDeviceItem
						{item}
						{now}
						{canManage}
						onDelete={onRemoveDevice}
						onEdit={onEditDevice}
					/>
				{/each}
			</div>
		{/if}
	</div>
</BaseCard>
