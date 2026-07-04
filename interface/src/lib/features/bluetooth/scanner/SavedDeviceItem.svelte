<script lang="ts">
	import Battery4 from '~icons/tabler/battery-4';
	import Battery3 from '~icons/tabler/battery-3';
	import Battery2 from '~icons/tabler/battery-2';
	import Battery1 from '~icons/tabler/battery-1';
	import Battery from '~icons/tabler/battery';
	import Signal from '~icons/tabler/antenna-bars-5';
	import Droplet from '~icons/tabler/droplet';
	import Trash from '~icons/tabler/trash';
	import Edit from '~icons/tabler/pencil';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton } from '$lib/components/shared/forms';

	interface ScannedDevice {
		mac: string;
		temp: number;
		humid: number;
		batt: number;
		rssi: number;
		lastSeen: number;
		alias?: string;
	}

	interface BleSensorConfig {
		mac: string;
		alias: string;
	}

	interface Props {
		item: {
			config: BleSensorConfig;
			data: ScannedDevice | undefined;
		};
		now: number;
		canManage?: boolean;
		onDelete: (_mac: string) => void;
		onEdit?: (_mac: string, _alias: string) => void;
	}

	let { item, now, canManage = true, onDelete, onEdit }: Props = $props();

	function getBatteryIcon(level: number) {
		if (level >= 90) return Battery4;
		if (level >= 60) return Battery3;
		if (level >= 40) return Battery2;
		if (level >= 10) return Battery1;
		return Battery;
	}

	function getFormatTimeAgo(ts: number, currentNow: number) {
		if (ts <= 0) {
			// Snapshot data keeps lastSeen at 0 until the device has trustworthy wall-clock
			// time. Showing "--" is clearer than rendering "54y ago" from a fake epoch,
			// and it matches the dashboard widget contract for the same sentinel.
			return '--';
		}

		// Guard against small skew between different snapshot/event sources.
		const ms = Math.max(0, currentNow - ts);
		if (ms < 5000) return m.time_just_now({ locale: i18n.languageTag });

		const seconds = Math.floor(ms / 1000);
		const minutes = Math.floor(seconds / 60);
		const hours = Math.floor(minutes / 60);

		if (hours > 0) return m.time_ago_h({ h: hours, m: minutes % 60 }, { locale: i18n.languageTag });
		if (minutes > 0)
			return m.time_ago_m({ m: minutes, s: seconds % 60 }, { locale: i18n.languageTag });
		return m.time_ago_s({ s: seconds }, { locale: i18n.languageTag });
	}
</script>

<ContentBox
	class="border border-base-300 p-3 flex flex-col gap-1 relative group hover:border-base-content/20 transition-colors"
>
	<!-- Row 1: Header (Name, MAC, Delete) -->
	<div class="flex justify-between items-start gap-2">
		<div class="min-w-0 flex flex-wrap items-baseline gap-x-2">
			<div class="font-bold text-sm truncate" title={item.config.alias}>{item.config.alias}</div>
			<div class="font-mono text-[11px] opacity-40 select-all">{item.config.mac}</div>
		</div>

		<div class="flex gap-0.5 -mt-1 -mr-1">
			<!-- Edit Action -->
			<!-- Edit Action -->
			<FormButton
				label=""
				icon={Edit}
				class="btn-ghost btn-xs btn-square text-base-content/40 hover:text-primary hover:bg-primary/10 transition-colors"
				onclick={() => onEdit?.(item.config.mac, item.config.alias)}
				disabled={!canManage}
				aria-label={m.action_edit({ locale: i18n.languageTag })}
				title={canManage
					? m.action_edit({ locale: i18n.languageTag })
					: m.menu_locked_admin({ locale: i18n.languageTag })}
			/>

			<!-- Delete Action -->
			<FormButton
				label=""
				icon={Trash}
				class="btn-ghost btn-xs btn-square text-base-content/40 hover:text-error hover:bg-error/10 transition-colors"
				onclick={() => onDelete(item.config.mac)}
				disabled={!canManage}
				aria-label={m.action_delete_device({ locale: i18n.languageTag })}
				title={canManage
					? m.action_delete_device({ locale: i18n.languageTag })
					: m.menu_locked_admin({ locale: i18n.languageTag })}
			/>
		</div>
	</div>

	<!-- Row 2: Data & Status -->
	{#if item.data}
		{@const BatteryIcon = getBatteryIcon(item.data.batt)}
		<div class="mt-2 flex items-start justify-between gap-3">
			<!-- Temp -->
			<div class="font-bold text-lg tabular-nums text-primary leading-none flex items-baseline">
				{item.data.temp.toFixed(1)}<span class="text-sm font-normal text-base-content/60 ml-0.5"
					>°C</span
				>
			</div>

			<!-- Time Ago -->
			<div class="text-[10px] opacity-30 whitespace-nowrap pt-0.5">
				{getFormatTimeAgo(item.data.lastSeen, now)}
			</div>
		</div>

		<div
			class="mt-2 grid grid-cols-3 gap-x-3 gap-y-2 sm:flex sm:flex-wrap sm:items-center sm:gap-4"
		>
			<!-- Humidity -->
			<div class="flex items-center gap-1.5 text-sm font-medium opacity-75">
				<Droplet class="w-3.5 h-3.5 opacity-70" />
				{item.data.humid}%
			</div>

			<!-- Battery -->
			<div
				class="flex items-center gap-1.5 text-sm font-medium {item.data.batt < 20
					? 'text-error'
					: item.data.batt < 50
						? 'text-warning'
						: 'text-success/80'}"
				title={m.tooltip_battery({ locale: i18n.languageTag })}
			>
				<BatteryIcon class="w-4 h-4" />
				<span class="font-mono">{item.data.batt}%</span>
			</div>

			<!-- RSSI -->
			<div
				class="flex items-center gap-1 text-sm font-medium {item.data.rssi > -60
					? 'text-success'
					: item.data.rssi > -80
						? 'text-warning'
						: 'text-error'}"
				title={m.tooltip_signal({ locale: i18n.languageTag })}
			>
				<Signal class="w-4 h-4" />
				<span class="font-mono">{item.data.rssi}</span>
			</div>
		</div>
	{:else}
		<div class="text-sm opacity-40 italic mt-1">
			{m.status_waiting({ locale: i18n.languageTag })}
		</div>
	{/if}
</ContentBox>
