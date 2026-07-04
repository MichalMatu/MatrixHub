<script lang="ts">
	import Wifi from '~icons/tabler/wifi';
	import Trash from '~icons/tabler/trash';
	import Pencil from '~icons/tabler/pencil';
	import Bolt from '~icons/tabler/bolt';
	import Activity from '~icons/tabler/activity';
	import Temperature from '~icons/tabler/temperature';
	import Gauge from '~icons/tabler/gauge';
	import Plug from '~icons/tabler/plug';

	import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton } from '$lib/components/shared/forms';

	interface Props {
		device: ShellyDevice;
		canManage: boolean;
		onToggle: (_state: boolean) => void;
		onEdit: () => void;
		onDelete: () => void;
	}

	let { device, canManage, onToggle, onEdit, onDelete }: Props = $props();
</script>

<ContentBox paddingClass="p-3" class="shadow-sm border border-base-200">
	<!-- Reduced padding -->
	<!-- Header Row: Status, Name, IP, Actions -->
	<div class="flex items-center gap-3">
		<!-- Status Icon -->
		<div
			class="flex flex-col items-center justify-center {device.isOnline
				? 'text-success'
				: 'text-base-content/30'}"
			title={device.isOnline ? m.status_online() : m.status_offline()}
		>
			<Wifi class="w-5 h-5" />
		</div>

		<!-- Name & Meta -->
		<div class="min-w-0 flex-1">
			<div class="flex items-center gap-2 flex-wrap">
				<h3 class="font-bold text-base leading-tight truncate">{device.name}</h3>
				<div class="badge badge-xs badge-ghost font-mono opacity-50">#{device.relay_index}</div>
				<div
					class="badge badge-xs font-bold uppercase tracking-wide {device.isOn
						? device.isOnline
							? 'badge-success'
							: 'badge-outline badge-success'
						: device.isOnline
							? 'badge-ghost'
							: 'badge-outline opacity-60'}"
				>
					{device.isOn
						? m.status_on({ locale: i18n.languageTag })
						: m.status_off({ locale: i18n.languageTag })}
				</div>
			</div>
			<div class="text-[10px] opacity-60 font-mono flex items-center gap-2">
				<span>{device.ip}</span>
				{#if device.rssi}
					<span class="opacity-30">|</span>
					<span title={m.tooltip_rssi()}>{device.rssi} dBm</span>
				{/if}
			</div>
		</div>

		<!-- Dynamic Power Badge (Visible even if collapsed) -->
		{#if device.isOnline && device.power !== undefined}
			<div class="hidden sm:flex flex-col items-end mr-2">
				<span
					class="font-mono font-bold text-lg leading-none {device.power === 0
						? 'opacity-40'
						: device.power < 100
							? 'text-success'
							: device.power < 2000
								? 'text-warning'
								: 'text-error'}"
					>{device.power.toFixed(1)}<span class="text-xs ml-0.5">W</span></span
				>
			</div>
		{/if}

		<!-- Actions -->
		<div class="flex items-center gap-1">
			<FormButton
				label=""
				icon={Plug}
				class="btn-sm btn-circle {device.isOn
					? 'btn-primary'
					: 'btn-ghost bg-base-200'} border-none shadow-none"
				onclick={() => onToggle(!device.isOn)}
				disabled={!canManage}
				title={device.isOn
					? m.action_turn_off({ locale: i18n.languageTag })
					: m.action_turn_on({ locale: i18n.languageTag })}
				ariaLabel={device.isOn
					? m.action_turn_off({ locale: i18n.languageTag })
					: m.action_turn_on({ locale: i18n.languageTag })}
			/>

			<FormButton
				label=""
				icon={Pencil}
				class="btn-ghost btn-circle btn-xs text-base-content/40 hover:text-primary"
				onclick={onEdit}
				disabled={!canManage}
				title={m.action_edit({ locale: i18n.languageTag })}
				ariaLabel={m.action_edit({ locale: i18n.languageTag })}
			/>

			<FormButton
				label=""
				icon={Trash}
				class="btn-ghost btn-circle btn-xs text-error/30 hover:text-error"
				onclick={onDelete}
				disabled={!canManage}
				title={m.action_delete({ locale: i18n.languageTag })}
				ariaLabel={m.action_delete({ locale: i18n.languageTag })}
			/>
		</div>
	</div>

	<!-- Compact Metrics Strip -->
	{#if device.isOnline && device.power !== undefined}
		<div
			class="flex flex-wrap items-center gap-x-4 gap-y-1 mt-2 pt-2 border-t border-base-200 text-xs"
		>
			<!-- Power (Mobile only) -->
			<div class="flex items-center gap-1 sm:hidden">
				<Bolt class="w-3.5 h-3.5" />
				<span class="font-mono font-bold">{device.power.toFixed(1)} W</span>
			</div>

			<!-- Voltage -->
			<div class="flex items-center gap-1 opacity-70" title={m.tooltip_voltage()}>
				<Bolt class="w-3.5 h-3.5 text-warning" />
				<span class="font-mono">{device.voltage?.toFixed(1) || '--'} V</span>
			</div>

			<!-- Current -->
			<div class="flex items-center gap-1 opacity-70" title={m.tooltip_current()}>
				<Activity class="w-3.5 h-3.5 text-info" />
				<span class="font-mono">{device.current?.toFixed(2) || '--'} A</span>
			</div>

			<!-- Temp -->
			<div class="flex items-center gap-1 opacity-70" title={m.tooltip_temperature()}>
				<Temperature
					class="w-3.5 h-3.5 {!device.temperature
						? ''
						: device.temperature < 40
							? 'text-success'
							: device.temperature < 60
								? 'text-warning'
								: 'text-error'}"
				/>
				<span class="font-mono">{device.temperature?.toFixed(1) || '--'} °C</span>
			</div>

			<div class="flex-1"></div>

			<!-- Energy -->
			<div class="flex items-center gap-1 opacity-50" title={m.tooltip_energy()}>
				<Gauge class="w-3 h-3" />
				<span class="font-mono">{device.energy?.toFixed(1) || 0} Wh</span>
			</div>
		</div>
	{/if}
</ContentBox>
