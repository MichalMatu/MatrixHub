<script lang="ts">
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import Plug from '~icons/tabler/plug';
	import { useAlarmShellySelect } from './useAlarmShellySelect.svelte';

	interface Props {
		shellyDeviceIds: string[];
		onShellyChange: (_ids: string[]) => void;
	}

	let { shellyDeviceIds, onShellyChange }: Props = $props();

	const shellyState = useAlarmShellySelect(
		() => shellyDeviceIds,
		(ids) => onShellyChange(ids)
	);
</script>

{#if shellyState.loading}
	<div class="flex items-center gap-2 text-xs text-base-content/60 py-2">
		<span class="loading loading-spinner loading-xs"></span>
		<span>{m.alarms_loading_shelly({ locale: i18n.languageTag })}</span>
	</div>
{:else if shellyState.error}
	<div class="text-xs text-error py-2">
		{m.error_prefix({ error: shellyState.error }, { locale: i18n.languageTag })}
	</div>
{:else if shellyState.devices.length > 0}
	<div class="flex flex-col gap-2 mt-2">
		<div class="flex items-center justify-between">
			<span class="text-xs font-bold uppercase tracking-wide opacity-70"
				>{m.alarms_field_shelly_devices({ locale: i18n.languageTag })}</span
			>
		</div>

		<div class="grid grid-cols-2 md:grid-cols-3 gap-3">
			{#each shellyState.devices as device (device.id)}
				<label
					class="cursor-pointer flex items-center gap-3 p-3 rounded-xl border transition-all duration-200 text-sm
						{shellyDeviceIds.includes(device.id)
						? 'bg-primary/5 border-primary shadow-sm'
						: 'bg-base-200/50 border-transparent hover:bg-base-200'}"
				>
					<input
						type="checkbox"
						checked={shellyDeviceIds.includes(device.id)}
						onchange={() => shellyState.toggleDevice(device.id)}
						class="checkbox checkbox-sm checkbox-primary rounded-[4px]"
					/>

					<div class="flex items-center gap-3 min-w-0">
						<div
							class="w-8 h-8 rounded-lg flex items-center justify-center shrink-0 {shellyDeviceIds.includes(
								device.id
							)
								? 'bg-primary text-primary-content'
								: 'bg-base-300 text-base-content/50'}"
						>
							<Plug class="w-4 h-4" />
						</div>

						<div class="flex flex-col min-w-0">
							<span class="font-medium leading-tight truncate">{device.name}</span>
						</div>
					</div>
				</label>
			{/each}
		</div>
	</div>
{:else}
	<div class="text-xs text-base-content/60 py-2">
		{m.alarms_field_shelly_hint({ locale: i18n.languageTag })}
	</div>
{/if}
