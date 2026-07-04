<script lang="ts">
	import Bluetooth from '~icons/tabler/bluetooth';
	import AlertCircle from '~icons/tabler/alert-circle';
	import type { AlarmSource } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { FormSelect } from '$lib/components/shared/forms';
	import { useBleDeviceSelect } from './useBleDeviceSelect.svelte';

	let {
		bleDeviceMac = $bindable(),
		source = 'ble_temperature'
	}: {
		bleDeviceMac?: string;
		source?: AlarmSource;
	} = $props();

	$effect(() => {
		if (bleDeviceMac === undefined) bleDeviceMac = '';
	});
	const bleState = useBleDeviceSelect(
		() => source,
		() => bleDeviceMac,
		(value) => {
			bleDeviceMac = value;
		}
	);
</script>

<div class="flex flex-col gap-1">
	<span class="text-xs font-bold flex items-center gap-1 opacity-70 uppercase tracking-wide">
		<Bluetooth class="w-3 h-3" />
		{m.alarms_field_ble_device({ locale: i18n.languageTag })}
	</span>

	{#if bleState.loading}
		<div class="flex justify-center p-2">
			<span class="loading loading-spinner loading-xs opacity-40"></span>
		</div>
	{:else if !bleState.bleEnabled}
		<div class="alert alert-warning py-1 text-xs px-2 min-h-0 flex items-center gap-2">
			<AlertCircle class="w-3 h-3" />
			<span>{m.alarms_ble_disabled({ locale: i18n.languageTag })}</span>
		</div>
	{:else if bleState.error}
		<div class="alert alert-error py-1 text-xs px-2 min-h-0 flex items-center gap-2">
			<AlertCircle class="w-3 h-3" />
			<span>{bleState.error}</span>
		</div>
	{:else if bleState.options.length === 1 && !bleState.loading}
		<div class="alert alert-info py-1 text-xs px-2 min-h-0 flex items-center gap-2">
			<Bluetooth class="w-3 h-3" />
			<span>{m.alarms_no_ble_devices({ locale: i18n.languageTag })}</span>
		</div>
	{:else}
		<FormSelect
			bind:value={bleDeviceMac}
			options={bleState.options}
			disabled={bleState.loading || !bleState.bleEnabled}
			class={!bleState.isValidSelection && (bleDeviceMac ?? '') !== ''
				? 'select-error select-sm w-full'
				: 'select-sm w-full'}
		/>
		{#if !bleState.isValidSelection && bleDeviceMac !== ''}
			<div class="text-[10px] text-error px-1 mt-0.5">
				{m.alarms_ble_device_not_found({ locale: i18n.languageTag })}
			</div>
		{/if}
	{/if}
</div>
